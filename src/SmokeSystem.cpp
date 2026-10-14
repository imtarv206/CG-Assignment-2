#include "SmokeSystem.h"

#include <algorithm>
#include <cmath>
#include <random>

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
