#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <filesystem>
#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

#include "Libs/Shader.h"

// ข้อมูลของควันหนึ่งก้อน (particle) ระหว่างที่มันยังมีชีวิตอยู่
struct SmokeParticle
{
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f, 1.0f, 0.0f};

    float age = 0.0f;
    float lifetime = 5.0f;

    float startSize = 0.1f;
    float endSize = 1.0f;

    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    float opacity = 0.25f;

    // การแกว่ง
    float swayPhase = 0.0f;

    bool isAlive() const
    {
        return age < lifetime;
    }

    float lifeRatio() const
    {
        if (lifetime <= 0.0f)
            return 1.0f;

        return std::clamp(age / lifetime, 0.0f, 1.0f);
    }
};

// จัดการวงจรชีวิตของควันทุกก้อน
class SmokeSystem
{
public:
    explicit SmokeSystem(const glm::vec3& emitterPosition = glm::vec3(0.0f));
    ~SmokeSystem();

    SmokeSystem(const SmokeSystem&) = delete;
    SmokeSystem& operator=(const SmokeSystem&) = delete;

    bool initialise(const std::filesystem::path& texturePath,
                    const std::filesystem::path& vertexShaderPath,
                    const std::filesystem::path& fragmentShaderPath);

    void setEmitterPosition(const glm::vec3& position);
    void update(float deltaTime, float elapsedTime);
    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& cameraRight,
                const glm::vec3& cameraUp,
                const glm::vec3& cameraPosition);

    const std::vector<SmokeParticle>& getParticles() const { return particles; }
    std::size_t getParticleCount() const { return particles.size(); }

private:
    void emitParticle();
    float randomRange(float minimum, float maximum);
    void releaseGraphicsResources();

    glm::vec3 emitterPosition{0.0f};
    std::vector<SmokeParticle> particles;
    std::mt19937 randomEngine;
    
    // ปล่อยควัน 25 ก้อนต่อวินาที และมีพร้อมกันได้ไม่เกิน 200 ก้อน
    float emissionAccumulator = 0.0f;
    float particlesPerSecond = 25.0f;
    std::size_t maxParticles = 200;

    Shader smokeShader;
    GLuint quadVao = 0;
    GLuint quadVbo = 0;
    GLuint quadEbo = 0;
    GLuint smokeTexture = 0;
    bool graphicsReady = false;

    GLint uniformView = -1;
    GLint uniformProjection = -1;
    GLint uniformCameraRight = -1;
    GLint uniformCameraUp = -1;
    GLint uniformParticleCenter = -1;
    GLint uniformParticleSize = -1;
    GLint uniformParticleRotation = -1;
    GLint uniformParticleOpacity = -1;
    GLint uniformSmokeColour = -1;
    GLint uniformSmokeTexture = -1;
};
