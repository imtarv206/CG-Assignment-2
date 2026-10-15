#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <cstddef>
#include <random>
#include <vector>

#include "Libs/Shader.h"

// จัดการเปลวไฟด้วยอนุภาค (Particle System) บน Billboard ที่หันเข้าหากล้อง
// ตำแหน่ง Emitter และขนาดของไฟใช้หน่วยเดียวกับพิกัดโลกของฉาก
class FlameRenderer
{
public:
    FlameRenderer() = default;
    ~FlameRenderer();

    FlameRenderer(const FlameRenderer&) = delete;
    FlameRenderer& operator=(const FlameRenderer&) = delete;

    // โหลด Shader และสร้าง Quad/Buffer สำหรับวาดอนุภาค
    bool initialise(const std::filesystem::path& vertexShaderPath,
                    const std::filesystem::path& fragmentShaderPath);

    // อัปเดตอายุ ตำแหน่ง สี และสร้างอนุภาคใหม่เมื่ออนุภาคเดิมหมดอายุ
    void update(float deltaTime,
                const glm::vec3& emitterPosition,
                float width,
                float height,
                const glm::vec3& cameraRight);

    // วาดอนุภาคทั้งหมด โดยให้ Quad หันเข้าหากล้อง
    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& cameraRight,
                const glm::vec3& cameraUp,
                float glowStrength = 0.85f);

private:
    struct Particle
    {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};
        glm::vec4 color{1.0f};
        glm::vec2 size{0.0f};
        float life = 0.0f;
        float maxLife = 1.0f;
        float phase = 0.0f;
        // ระยะลอยขึ้นสะสมและเพดานความสูงของอนุภาค ใช้ทำให้ไฟกลางสูงกว่าด้านข้าง
        float riseDistance = 0.0f;
        float riseLimit = 1.0f;
    };

    // ข้อมูลต่ออนุภาคที่ส่งไปยัง GPU ในหนึ่ง Instance
    struct ParticleInstance
    {
        glm::vec3 position;
        glm::vec2 size;
        glm::vec4 color;
    };

    static constexpr std::size_t MaxParticles = 280;
    static constexpr float EmissionRate = 220.0f;

    float randomRange(float minimum, float maximum);
    void respawnParticle(Particle& particle,
                         const glm::vec3& emitterPosition,
                         float width,
                         float height,
                         const glm::vec3& cameraRight,
                         bool distributeAlongFlame);
    void releaseGraphicsResources();

    Shader flameShader;
    GLuint quadVao = 0;
    GLuint quadVbo = 0;
    GLuint instanceVbo = 0;
    bool graphicsReady = false;
    bool initialPopulationDone = false;

    float emissionAccumulator = 0.0f;
    std::size_t nextParticleIndex = 0;
    std::vector<Particle> particles;
    std::vector<ParticleInstance> instanceData;
    std::mt19937 randomEngine{std::random_device{}()};

    GLint uniformView = -1;
    GLint uniformProjection = -1;
    GLint uniformCameraRight = -1;
    GLint uniformCameraUp = -1;
    GLint uniformGlowStrength = -1;
};
