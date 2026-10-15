#include "FlameRenderer.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace
{
    // ลดขนาดโดยรวมของเปลวไฟเหลือ 80% จากเดิม
    constexpr float FlameOverallSizeScale = 0.80f;

    // ลดความกว้างของเปลวไฟลงอีก 5% จากเวอร์ชันก่อนหน้า โดยไม่ลดความสูง
    constexpr float FlameWidthScale = 0.95f;

    // ลดความเร็วพื้นฐานของอนุภาคลง 5% จากเดิม
    constexpr float ParticleSpeedScale = 0.95f;

    // เมื่ออนุภาคลอยขึ้นใกล้ปลายไฟ ให้ขนาดเล็กลงเหลือ 35% ของขนาดตอนอยู่ฐาน
    // ค่านี้ปรับได้โดยไม่กระทบ LifetimeScale หรือ WidthScale
    constexpr float ParticleTipSizeScale = 0.15f;

    // ปรับอายุให้สั้นลงเพื่อให้ระยะทางลอยโดยรวมสอดคล้องกับการลดขนาด 20%
    // ความเร็ว 0.95 x อายุ 0.80/0.95 = ระยะทางประมาณ 80% ของเวอร์ชันก่อนหน้า
    constexpr float ParticleLifetimeScale = FlameOverallSizeScale / ParticleSpeedScale;

    // อนุภาคตรงกลางให้อยู่ได้นานกว่า จึงลอยสูงกว่ากลุ่มด้านข้าง
    // ค่านี้เป็นตัวคูณอายุ ไม่ได้เปลี่ยนความเร็วพื้นฐาน 95% ของอนุภาค
    constexpr float CenterParticleLifetimeScale = 1.20f;
    constexpr float SideParticleLifetimeScale = 0.70f;

    // จำกัดความสูงสูงสุดตามตำแหน่งด้านข้าง: กลางสูงกว่า ด้านข้างต่ำกว่า
    // คูณกับ fireHeight และ FlameOverallSizeScale จึงยังยึดขนาดไฟที่ผู้ใช้กำหนด
    constexpr float CenterRiseLimitScale = 1.12f;
    constexpr float SideRiseLimitScale = 0.72f;

    // ฟังก์ชัน Smoothstep สำหรับค่อย ๆ เปลี่ยนค่าระหว่าง 0 ถึง 1
    float smoothStep(float edge0, float edge1, float value)
    {
        const float t = glm::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

FlameRenderer::~FlameRenderer()
{
    releaseGraphicsResources();
}

bool FlameRenderer::initialise(
    const std::filesystem::path& vertexShaderPath,
    const std::filesystem::path& fragmentShaderPath)
{
    releaseGraphicsResources();

    // Quad หนึ่งชิ้นมีสองสามเหลี่ยม โดยตำแหน่งอยู่ในช่วง -0.5 ถึง 0.5
    // ตำแหน่งและขนาดของอนุภาคแต่ละตัวจะถูกส่งแยกกันด้วย Instance Buffer
    constexpr float vertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    glGenVertexArrays(1, &quadVao);
    glGenBuffers(1, &quadVbo);
    glGenBuffers(1, &instanceVbo);

    glBindVertexArray(quadVao);

    // ข้อมูลตำแหน่งมุม Quad ซึ่งเหมือนกันสำหรับอนุภาคทุกตัว
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // Buffer ที่เก็บตำแหน่ง ขนาด และสีของแต่ละอนุภาค
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(MaxParticles * sizeof(ParticleInstance)),
                 nullptr,
                 GL_STREAM_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(ParticleInstance),
                          reinterpret_cast<void*>(offsetof(ParticleInstance, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(ParticleInstance),
                          reinterpret_cast<void*>(offsetof(ParticleInstance, size)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE,
                          sizeof(ParticleInstance),
                          reinterpret_cast<void*>(offsetof(ParticleInstance, color)));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    flameShader.CreateFromFiles(vertexShaderPath, fragmentShaderPath);
    uniformView = flameShader.GetUniformLocation("view");
    uniformProjection = flameShader.GetUniformLocation("projection");
    uniformCameraRight = flameShader.GetUniformLocation("cameraRight");
    uniformCameraUp = flameShader.GetUniformLocation("cameraUp");
    uniformGlowStrength = flameShader.GetUniformLocation("glowStrength");

    graphicsReady =
        quadVao != 0 && quadVbo != 0 && instanceVbo != 0 &&
        uniformView >= 0 && uniformProjection >= 0 &&
        uniformCameraRight >= 0 && uniformCameraUp >= 0 &&
        uniformGlowStrength >= 0;

    if (!graphicsReady)
    {
        std::cerr << "[ERROR] Flame particle shader or OpenGL buffers are not ready.\n";
        releaseGraphicsResources();
        return false;
    }

    particles.assign(MaxParticles, Particle{});
    instanceData.clear();
    instanceData.reserve(MaxParticles);
    emissionAccumulator = 0.0f;
    nextParticleIndex = 0;
    initialPopulationDone = false;
    return true;
}

float FlameRenderer::randomRange(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(randomEngine);
}

void FlameRenderer::respawnParticle(
    Particle& particle,
    const glm::vec3& emitterPosition,
    float width,
    float height,
    const glm::vec3& cameraRight,
    bool distributeAlongFlame)
{
    // สุ่มตำแหน่งด้านข้างหนึ่งครั้ง เพื่อใช้กำหนดทั้งตำแหน่งเริ่มต้นและความสูงของอนุภาค
    // ค่าที่ใกล้ 0 คือบริเวณกลางไฟ ส่วนค่าใกล้ -1 หรือ +1 คือบริเวณด้านข้าง
    const float lateralPosition = randomRange(-1.0f, 1.0f);
    const float centerWeight = 1.0f - std::fabs(lateralPosition);

    // อนุภาคกลางมีอายุนานกว่า ส่วนอนุภาคด้านข้างหมดอายุเร็วกว่า
    // จึงเกิดแนวไฟที่สูงตรงกลางและเตี้ยลงทางซ้าย/ขวา
    const float lifetimeProfile = glm::mix(
        SideParticleLifetimeScale,
        CenterParticleLifetimeScale,
        centerWeight);
    particle.maxLife = randomRange(0.42f, 0.78f)
                     * ParticleLifetimeScale
                     * lifetimeProfile;
    particle.life = distributeAlongFlame
        ? randomRange(particle.maxLife * 0.25f, particle.maxLife)
        : particle.maxLife;

    // ขนาดแกน X ลดลง 20% ตามเดิม และลดความกว้างเพิ่มอีก 3%
    // ขนาดแกน Y คงการลดขนาดโดยรวม 20% แต่ไม่ลดเพิ่มอีก 3%
    particle.size.x = randomRange(width * 0.08f, width * 0.21f)
                    * FlameOverallSizeScale * FlameWidthScale;
    particle.size.y = randomRange(height * 0.22f, height * 0.50f)
                    * FlameOverallSizeScale;
    particle.phase = randomRange(0.0f, 6.2831853f);

    // กำหนดระยะลอยขึ้นสูงสุดตามตำแหน่ง: อนุภาคกลางมีเพดานสูงกว่าด้านข้าง
    particle.riseLimit = height * FlameOverallSizeScale * glm::mix(
        SideRiseLimitScale,
        CenterRiseLimitScale,
        centerWeight);
    particle.riseDistance = 0.0f;

    // อนุภาคกระจายจากฐาน ลอยขึ้นตามแกน Y และแกว่งด้านข้างเล็กน้อย
    // ความเร็วพื้นฐานทุกแกนยังคูณ 0.95 เพื่อให้ช้าลง 5% ตามค่าก่อนหน้า
    const glm::vec3 initialVelocity =
        cameraRight * (lateralPosition * width * 0.14f * FlameWidthScale) +
        glm::vec3(randomRange(-width * 0.035f, width * 0.035f) * FlameWidthScale,
                  randomRange(height * 0.90f, height * 1.65f),
                  randomRange(-width * 0.055f, width * 0.055f) * FlameWidthScale);
    particle.velocity = initialVelocity * ParticleSpeedScale;

    // ลดการกระจายทางแนวนอนที่ฐานลงเพิ่มอีก 3% โดยไม่เปลี่ยนความสูงของจุดกำเนิด
    particle.position = emitterPosition +
        cameraRight * (lateralPosition * width * 0.18f
                       * FlameOverallSizeScale * FlameWidthScale) +
        glm::vec3(randomRange(-width * 0.035f, width * 0.035f)
                      * FlameOverallSizeScale * FlameWidthScale,
                  0.0f,
                  randomRange(-width * 0.045f, width * 0.045f)
                      * FlameOverallSizeScale * FlameWidthScale);

    // เติมอนุภาคหลายช่วงอายุทันทีตอนเริ่ม เพื่อไม่ให้ไฟค่อย ๆ ก่อตัวจากฐาน
    if (distributeAlongFlame)
    {
        float elapsedLife = particle.maxLife - particle.life;
        if (particle.velocity.y > 0.0f)
        {
            // ให้ตำแหน่งเริ่มต้นอยู่ต่ำกว่าขีดจำกัดความสูงของอนุภาคเล็กน้อย
            const float initialRiseLimit = particle.riseLimit * randomRange(0.45f, 0.82f);
            elapsedLife = std::min(elapsedLife,
                                   initialRiseLimit / particle.velocity.y);
            particle.life = particle.maxLife - elapsedLife;
        }

        particle.position += particle.velocity * elapsedLife;
        particle.riseDistance = std::max(0.0f, particle.velocity.y * elapsedLife);
    }

    // สีเริ่มต้นเป็นเหลืองสว่าง ก่อนค่อย ๆ เปลี่ยนเป็นส้มเมื่ออนุภาคมีอายุมากขึ้น
    particle.color = glm::vec4(1.0f, 0.82f, 0.22f, 1.0f);
}

void FlameRenderer::update(
    float deltaTime,
    const glm::vec3& emitterPosition,
    float width,
    float height,
    const glm::vec3& cameraRight)
{
    if (!graphicsReady || width <= 0.0f || height <= 0.0f)
        return;

    // จำกัด delta time เพื่อป้องกันอนุภาคกระโดดไกลหลังเฟรมสะดุด
    const float dt = glm::clamp(deltaTime, 0.0f, 0.05f);

    // สร้างอนุภาคชุดแรกกระจายตามแนวสูงของเปลวไฟ ลดอาการไฟค่อย ๆ ก่อตัวตอนเริ่ม
    if (!initialPopulationDone)
    {
        for (Particle& particle : particles)
        {
            respawnParticle(particle, emitterPosition, width, height,
                            cameraRight, true);
        }
        initialPopulationDone = true;
    }

    // เคลื่อนอนุภาคที่ยังมีชีวิต พร้อมเปลี่ยนสีและค่อย ๆ จางเมื่อใกล้หมดอายุ
    for (Particle& particle : particles)
    {
        if (particle.life <= 0.0f)
            continue;

        particle.life -= dt;
        if (particle.life <= 0.0f)
            continue;

        const float age = glm::clamp(1.0f - particle.life / particle.maxLife,
                                     0.0f, 1.0f);
        particle.position += particle.velocity * dt;
        particle.position += cameraRight *
            (std::sin(age * 10.0f + particle.phase) * width * 0.035f
             * ParticleSpeedScale * FlameWidthScale * dt);
        particle.riseDistance += std::max(particle.velocity.y, 0.0f) * dt;

        // จบอายุเมื่อถึงเพดานความสูงของอนุภาค เพื่อรักษารูปทรงสูงตรงกลางและเตี้ยด้านข้าง
        if (particle.riseDistance >= particle.riseLimit)
        {
            particle.life = 0.0f;
            continue;
        }

        // อนุภาคใหม่ออกเหลืองสว่าง ส่วนอนุภาคที่สูงขึ้นจะเปลี่ยนเป็นส้มแดง
        const glm::vec3 baseYellow(1.0f, 0.84f, 0.25f);
        const glm::vec3 olderOrange(1.0f, 0.20f, 0.025f);
        const glm::vec3 particleRgb = glm::mix(baseYellow, olderOrange,
                                               smoothStep(0.05f, 0.95f, age));

        // ค่อย ๆ จางเมื่ออายุใกล้หมด และจางเพิ่มเมื่อใกล้ถึงเพดานความสูง
        const float ageFade = 1.0f - smoothStep(0.66f, 1.0f, age);
        const float heightRatio = particle.riseDistance / particle.riseLimit;
        const float heightFade = 1.0f - smoothStep(0.72f, 1.0f, heightRatio);
        particle.color = glm::vec4(particleRgb, ageFade * heightFade);
    }

    // สร้างอนุภาคตามอัตราคงที่ต่อวินาที ไม่ผูกกับจำนวน FPS ของเครื่อง
    // เนื่องจากลดอายุอนุภาค จึงเพิ่มอัตราการเกิดตามสัดส่วน เพื่อให้ความหนาแน่นของไฟใกล้เคียงเดิม
    emissionAccumulator += dt * (EmissionRate / ParticleLifetimeScale);
    const std::size_t spawnCount = static_cast<std::size_t>(emissionAccumulator);
    emissionAccumulator -= static_cast<float>(spawnCount);

    for (std::size_t spawned = 0; spawned < spawnCount; ++spawned)
    {
        bool foundUnused = false;

        // ค้นหาอนุภาคหมดอายุแบบวนต่อจากตำแหน่งที่ใช้ครั้งก่อน
        for (std::size_t checked = 0; checked < particles.size(); ++checked)
        {
            const std::size_t index = (nextParticleIndex + checked) % particles.size();
            if (particles[index].life <= 0.0f)
            {
                respawnParticle(particles[index], emitterPosition, width, height,
                                cameraRight, false);
                nextParticleIndex = (index + 1) % particles.size();
                foundUnused = true;
                break;
            }
        }

        // ถ้าอนุภาคยังมีชีวิตเต็มจำนวน จะข้ามการเกิดใหม่ในรอบนี้
        if (!foundUnused)
            break;
    }
}

void FlameRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraRight,
    const glm::vec3& cameraUp,
    float glowStrength)
{
    if (!graphicsReady)
        return;

    // เตรียมข้อมูลเฉพาะอนุภาคที่ยังมีชีวิต เพื่อนำไปวาดด้วย Instanced Rendering
    instanceData.clear();
    for (const Particle& particle : particles)
    {
        if (particle.life <= 0.0f || particle.color.a <= 0.005f)
            continue;

        // ใช้ระยะที่อนุภาคลอยขึ้นเทียบกับเพดานความสูง เพื่อย่อขนาดตามตำแหน่งจริง
        // ค่า 0 หมายถึงฐานไฟ ส่วนค่า 1 หมายถึงใกล้เพดานความสูงของอนุภาค
        const float riseRatio = glm::clamp(
            particle.riseDistance / std::max(particle.riseLimit, 0.001f),
            0.0f,
            1.0f);

        // ค่อย ๆ เปลี่ยนจากขนาดเต็มที่ฐาน ไปเหลือ ParticleTipSizeScale ที่ปลายไฟ
        // ใช้ Smoothstep เพื่อให้การย่อดูต่อเนื่อง ไม่หดตัวเป็นขั้น ๆ
        const float taperProgress = smoothStep(0.0f, 1.0f, riseRatio);
        const float sizeScale = glm::mix(1.0f, ParticleTipSizeScale, taperProgress);

        instanceData.push_back({
            particle.position,
            particle.size * sizeScale,
            particle.color
        });
    }

    if (instanceData.empty())
        return;

    // บันทึก Blend และ Depth Write เดิม แล้วคืนค่าหลังวาดอนุภาคเสร็จ
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean previousDepthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

    GLint previousBlendSrcRgb = GL_SRC_ALPHA;
    GLint previousBlendDstRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    flameShader.UseShader();
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniformCameraRight, 1, glm::value_ptr(cameraRight));
    glUniform3fv(uniformCameraUp, 1, glm::value_ptr(cameraUp));
    glUniform1f(uniformGlowStrength, glm::clamp(glowStrength, 0.0f, 2.0f));

    // Additive blending ทำให้อนุภาคซ้อนกันแล้วเกิดแสงเรือง แต่ยังใช้ Depth Test บังผนังได้
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(instanceData.size() * sizeof(ParticleInstance)),
                    instanceData.data());

    glBindVertexArray(quadVao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6,
                          static_cast<GLsizei>(instanceData.size()));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDepthMask(previousDepthMask);
    glBlendFuncSeparate(static_cast<GLenum>(previousBlendSrcRgb),
                        static_cast<GLenum>(previousBlendDstRgb),
                        static_cast<GLenum>(previousBlendSrcAlpha),
                        static_cast<GLenum>(previousBlendDstAlpha));
    if (!blendWasEnabled)
        glDisable(GL_BLEND);
}

void FlameRenderer::releaseGraphicsResources()
{
    graphicsReady = false;
    initialPopulationDone = false;
    particles.clear();
    instanceData.clear();

    if (instanceVbo != 0)
    {
        glDeleteBuffers(1, &instanceVbo);
        instanceVbo = 0;
    }
    if (quadVbo != 0)
    {
        glDeleteBuffers(1, &quadVbo);
        quadVbo = 0;
    }
    if (quadVao != 0)
    {
        glDeleteVertexArrays(1, &quadVao);
        quadVao = 0;
    }

    flameShader.ClearShader();
}
