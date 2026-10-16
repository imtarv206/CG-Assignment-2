#include "SkyDome.h"

#include <cmath>
#include <iostream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace
{
    constexpr float PI = 3.14159265358979323846f;
}

SkyDome::~SkyDome()
{
    releaseGraphicsResources();
}

bool SkyDome::initialise(float radius,
                         unsigned int sectorCount,
                         unsigned int stackCount)
{
    releaseGraphicsResources();

    if (radius <= 0.0f || sectorCount < 3 || stackCount < 1)
    {
        std::cerr << "SkyDome requires a positive radius, at least 3 sectors, "
                  << "and at least 1 stack.\n";
        return false;
    }

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(
        static_cast<std::size_t>(stackCount + 1) *
        static_cast<std::size_t>(sectorCount + 1) * 3);
    indices.reserve(
        static_cast<std::size_t>(stackCount) *
        static_cast<std::size_t>(sectorCount) * 6);

    const float sectorStep = 2.0f * PI / static_cast<float>(sectorCount);
    const float elevationStep = 0.5f * PI / static_cast<float>(stackCount);

    // วงล่างเริ่มที่เส้นขอบฟ้า (elevation = 0) และวงบนสุดจบที่ยอดโดม 90 องศา
    for (unsigned int stack = 0; stack <= stackCount; ++stack)
    {
        const float elevation = static_cast<float>(stack) * elevationStep;
        const float horizontalRadius = radius * std::cos(elevation);
        const float y = radius * std::sin(elevation);

        // สร้าง vertex ซ้ำหนึ่งจุดท้ายวงเพื่อปิดรอยต่อ 0/360 องศา
        for (unsigned int sector = 0; sector <= sectorCount; ++sector)
        {
            const float azimuth = static_cast<float>(sector) * sectorStep;
            vertices.push_back(horizontalRadius * std::cos(azimuth));
            vertices.push_back(y);
            vertices.push_back(horizontalRadius * std::sin(azimuth));
        }
    }

    const unsigned int verticesPerRing = sectorCount + 1;
    for (unsigned int stack = 0; stack < stackCount; ++stack)
    {
        for (unsigned int sector = 0; sector < sectorCount; ++sector)
        {
            const unsigned int lowerLeft = stack * verticesPerRing + sector;
            const unsigned int lowerRight = lowerLeft + 1;
            const unsigned int upperLeft = lowerLeft + verticesPerRing;
            const unsigned int upperRight = upperLeft + 1;

            // เรียง winding ให้ด้านหน้าของสามเหลี่ยมหันเข้าหากลางโดม
            indices.push_back(lowerLeft);
            indices.push_back(lowerRight);
            indices.push_back(upperLeft);

            indices.push_back(lowerRight);
            indices.push_back(upperRight);
            indices.push_back(upperLeft);
        }
    }

    vertexCount = vertices.size() / 3;
    indexCount = indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return vao != 0 && vbo != 0 && ebo != 0;
}

bool SkyDome::initialiseShaders(
    const std::filesystem::path& vertexShaderPath,
    const std::filesystem::path& fragmentShaderPath)
{
    skyShader.ClearShader();
    shaderReady = false;

    skyShader.CreateFromFiles(vertexShaderPath, fragmentShaderPath);
    uniformView = skyShader.GetUniformLocation("view");
    uniformProjection = skyShader.GetUniformLocation("projection");
    uniformHorizonColour = skyShader.GetUniformLocation("horizonColour");
    uniformZenithColour = skyShader.GetUniformLocation("zenithColour");
    uniformCloudColour = skyShader.GetUniformLocation("cloudColour");
    uniformTime = skyShader.GetUniformLocation("time");

    shaderReady =
        uniformView >= 0 && uniformProjection >= 0 &&
        uniformHorizonColour >= 0 && uniformZenithColour >= 0 &&
        uniformCloudColour >= 0 && uniformTime >= 0;

    if (!shaderReady)
        std::cerr << "SkyDome shader is missing one or more required uniforms.\n";

    return shaderReady;
}

void SkyDome::render(const glm::mat4& view,
                     const glm::mat4& projection,
                     const glm::vec3& horizonColour,
                     const glm::vec3& zenithColour,
                     const glm::vec3& cloudColour,
                     float elapsedTime)
{
    if (!shaderReady || vao == 0 || indexCount == 0)
        return;

    skyShader.UseShader();
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniformHorizonColour, 1, glm::value_ptr(horizonColour));
    glUniform3fv(uniformZenithColour, 1, glm::value_ptr(zenithColour));
    glUniform3fv(uniformCloudColour, 1, glm::value_ptr(cloudColour));
    glUniform1f(uniformTime, elapsedTime);

    // Vertex shader วางโดมที่ depth = 1 จึงใช้ LEQUAL และปิดการเขียน depth
    // วัตถุทึบที่วาดก่อนหน้าจะบังท้องฟ้า ส่วนควันจะถูกวาดทับในลำดับถัดไป
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glBindVertexArray(vao);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(indexCount),
        GL_UNSIGNED_INT,
        nullptr);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void SkyDome::releaseGraphicsResources()
{
    shaderReady = false;
    skyShader.ClearShader();

    if (ebo != 0)
    {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
    if (vbo != 0)
    {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0)
    {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    vertexCount = 0;
    indexCount = 0;

    uniformView = -1;
    uniformProjection = -1;
    uniformHorizonColour = -1;
    uniformZenithColour = -1;
    uniformCloudColour = -1;
    uniformTime = -1;
}
