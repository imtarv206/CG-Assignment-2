#include "SmokeSystem.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <string>

#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace
{
    constexpr float TWO_PI = 6.28318530718f;
}

SmokeSystem::SmokeSystem(const glm::vec3& initialEmitterPosition)
    : emitterPosition(initialEmitterPosition),
      randomEngine(std::random_device{}())
{
    particles.reserve(maxParticles);
}

SmokeSystem::~SmokeSystem()
{
    releaseGraphicsResources();
}

bool SmokeSystem::initialise(const std::filesystem::path& texturePath,
                             const std::filesystem::path& vertexShaderPath,
                             const std::filesystem::path& fragmentShaderPath)
{
    releaseGraphicsResources();

    // Quad หนึ่งแผ่น: ตำแหน่งมุม X Y ตามด้วยพิกัด U V
    constexpr float vertices[] = {
        -0.5f, -0.5f, 0.0f, 0.0f,
         0.5f, -0.5f, 1.0f, 0.0f,
         0.5f,  0.5f, 1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f, 1.0f
    };
    constexpr unsigned int indices[] = {0, 1, 2, 2, 3, 0};

    glGenVertexArrays(1, &quadVao);
    glGenBuffers(1, &quadVbo);
    glGenBuffers(1, &quadEbo);

    glBindVertexArray(quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    int width = 0;
    int height = 0;
    int channels = 0;
    const std::string textureFilename = texturePath.string();
    unsigned char* pixels = stbi_load(
        textureFilename.c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);

    if (pixels == nullptr)
    {
        std::cerr << "Failed to load smoke texture: " << texturePath << '\n';
        releaseGraphicsResources();
        return false;
    }

    glGenTextures(1, &smokeTexture);
    glBindTexture(GL_TEXTURE_2D, smokeTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    smokeShader.CreateFromFiles(vertexShaderPath, fragmentShaderPath);
    uniformView = smokeShader.GetUniformLocation("view");
    uniformProjection = smokeShader.GetUniformLocation("projection");
    uniformCameraRight = smokeShader.GetUniformLocation("cameraRight");
    uniformCameraUp = smokeShader.GetUniformLocation("cameraUp");
    uniformParticleCenter = smokeShader.GetUniformLocation("particleCenter");
    uniformParticleSize = smokeShader.GetUniformLocation("particleSize");
    uniformParticleRotation = smokeShader.GetUniformLocation("particleRotation");
    uniformParticleOpacity = smokeShader.GetUniformLocation("particleOpacity");
    uniformSmokeColour = smokeShader.GetUniformLocation("smokeColour");
    uniformSmokeTexture = smokeShader.GetUniformLocation("smokeTexture");
    uniformViewPos = smokeShader.GetUniformLocation("viewPos");
    uniformFogDensity = smokeShader.GetUniformLocation("fogDensity");

    const bool uniformsReady =
        uniformView >= 0 && uniformProjection >= 0 &&
        uniformCameraRight >= 0 && uniformCameraUp >= 0 &&
        uniformParticleCenter >= 0 && uniformParticleSize >= 0 &&
        uniformParticleRotation >= 0 && uniformParticleOpacity >= 0 &&
        uniformSmokeColour >= 0 && uniformSmokeTexture >= 0 &&
        uniformViewPos >= 0 && uniformFogDensity >= 0;

    if (!uniformsReady)
    {
        std::cerr << "Smoke shader is missing one or more required uniforms.\n";
        releaseGraphicsResources();
        return false;
    }

    graphicsReady = true;
    return true;
}

void SmokeSystem::setEmitterPosition(const glm::vec3& position)
{
    emitterPosition = position;
}

void SmokeSystem::update(float deltaTime, float elapsedTime)
{
    if (deltaTime <= 0.0f)
        return;

    // อัปเดตควันที่มีอยู่แล้วให้ลอย หมุน และแกว่งอย่างไม่พร้อมกัน
    for (SmokeParticle& particle : particles)
    {
        particle.age += deltaTime;
        particle.position += particle.velocity * deltaTime;
        particle.rotation += particle.rotationSpeed * deltaTime;

        const float sway = std::sin(elapsedTime * 1.4f + particle.swayPhase);
        particle.position.x += sway * 0.05f * deltaTime;
        particle.position.z += std::cos(elapsedTime + particle.swayPhase) * 0.035f * deltaTime;
    }

    // ลบอนุภาคที่หมดอายุออกจาก vector
    particles.erase(
        std::remove_if(
            particles.begin(),
            particles.end(),
            [](const SmokeParticle& particle) { return !particle.isAlive(); }),
        particles.end());

    // สะสมเศษของจำนวนอนุภาค ทำให้อัตราปล่อยไม่ขึ้นกับ FPS
    emissionAccumulator += particlesPerSecond * deltaTime;
    while (emissionAccumulator >= 1.0f && particles.size() < maxParticles)
    {
        emitParticle();
        emissionAccumulator -= 1.0f;
    }

    // เมื่อเต็มความจุ ไม่สะสมคิวขนาดใหญ่จนเกิดการปล่อยเป็นก้อนภายหลัง
    if (particles.size() >= maxParticles)
        emissionAccumulator = std::min(emissionAccumulator, 1.0f);
}

void SmokeSystem::render(const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::vec3& cameraRight,
                         const glm::vec3& cameraUp,
                         const glm::vec3& cameraPosition,
                         float fogDensity)
{
    if (!graphicsReady || particles.empty())
        return;

    // Alpha blending ต้องวาดวัตถุโปร่งใสจากไกลเข้ามาหากล้อง
    std::vector<const SmokeParticle*> sortedParticles;
    sortedParticles.reserve(particles.size());
    for (const SmokeParticle& particle : particles)
        sortedParticles.push_back(&particle);

    std::sort(
        sortedParticles.begin(),
        sortedParticles.end(),
        [&cameraPosition](const SmokeParticle* left, const SmokeParticle* right)
        {
            const glm::vec3 leftOffset = left->position - cameraPosition;
            const glm::vec3 rightOffset = right->position - cameraPosition;
            return glm::dot(leftOffset, leftOffset) > glm::dot(rightOffset, rightOffset);
        });

    smokeShader.UseShader();
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniformCameraRight, 1, glm::value_ptr(cameraRight));
    glUniform3fv(uniformCameraUp, 1, glm::value_ptr(cameraUp));
    // ใช้โทนเดียวกับหมอกและท้องฟ้า เพื่อไม่ให้ควันดูแยกจากฉาก
    glUniform3f(uniformSmokeColour, 0.35f, 0.43f, 0.46f);
    glUniform1i(uniformSmokeTexture, 0);
    glUniform3fv(uniformViewPos, 1, glm::value_ptr(cameraPosition));
    glUniform1f(uniformFogDensity, fogDensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smokeTexture);
    glBindVertexArray(quadVao);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (const SmokeParticle* particle : sortedParticles)
    {
        const float life = particle->lifeRatio();
        const float smoothLife = life * life * (3.0f - 2.0f * life);
        const float size = particle->startSize +
            (particle->endSize - particle->startSize) * smoothLife;

        const float fadeIn = std::clamp(life / 0.12f, 0.0f, 1.0f);
        const float fadeOut = 1.0f - std::clamp((life - 0.55f) / 0.45f, 0.0f, 1.0f);
        const float opacity = particle->opacity * fadeIn * fadeOut;

        glUniform3fv(uniformParticleCenter, 1, glm::value_ptr(particle->position));
        glUniform1f(uniformParticleSize, size);
        glUniform1f(uniformParticleRotation, particle->rotation);
        glUniform1f(uniformParticleOpacity, opacity);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SmokeSystem::emitParticle()
{
    SmokeParticle particle;

    particle.position = emitterPosition + glm::vec3(
        randomRange(-0.04f, 0.04f),
        randomRange(0.0f, 0.03f),
        randomRange(-0.04f, 0.04f));

    particle.velocity = glm::vec3(
        randomRange(-0.06f, 0.06f),
        randomRange(0.35f, 0.65f),
        randomRange(-0.06f, 0.06f));

    particle.lifetime = randomRange(4.0f, 7.0f);
    particle.startSize = randomRange(0.08f, 0.14f);
    particle.endSize = randomRange(0.7f, 1.2f);
    particle.rotation = randomRange(0.0f, TWO_PI);
    particle.rotationSpeed = randomRange(-0.7f, 0.7f);
    particle.opacity = randomRange(0.15f, 0.30f);
    particle.swayPhase = randomRange(0.0f, TWO_PI);

    particles.push_back(particle);
}

float SmokeSystem::randomRange(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(randomEngine);
}

void SmokeSystem::releaseGraphicsResources()
{
    graphicsReady = false;

    if (smokeTexture != 0)
    {
        glDeleteTextures(1, &smokeTexture);
        smokeTexture = 0;
    }
    if (quadEbo != 0)
    {
        glDeleteBuffers(1, &quadEbo);
        quadEbo = 0;
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

    smokeShader.ClearShader();
}
