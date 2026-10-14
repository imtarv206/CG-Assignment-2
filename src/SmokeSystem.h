#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

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

    void setEmitterPosition(const glm::vec3& position);
    void update(float deltaTime, float elapsedTime);

    const std::vector<SmokeParticle>& getParticles() const { return particles; }
    std::size_t getParticleCount() const { return particles.size(); }

private:
    void emitParticle();
    float randomRange(float minimum, float maximum);

    glm::vec3 emitterPosition{0.0f};
    std::vector<SmokeParticle> particles;
    std::mt19937 randomEngine;
    
    // ปล่อยควัน 25 ก้อนต่อวินาที และมีพร้อมกันได้ไม่เกิน 200 ก้อน
    float emissionAccumulator = 0.0f;
    float particlesPerSecond = 25.0f;
    std::size_t maxParticles = 200;
};
