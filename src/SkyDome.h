#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <cstddef>
#include <filesystem>

#include "Libs/Shader.h"

// Step 1: สร้างและเก็บ geometry ครึ่งทรงกลมสำหรับท้องฟ้า
// ขั้นนี้ยังไม่ผูก shader และยังไม่วาดลงหน้าจอ
class SkyDome
{
public:
    SkyDome() = default;
    ~SkyDome();

    SkyDome(const SkyDome&) = delete;
    SkyDome& operator=(const SkyDome&) = delete;

    bool initialise(float radius,
                    unsigned int sectorCount,
                    unsigned int stackCount);

    bool initialiseShaders(const std::filesystem::path& vertexShaderPath,
                           const std::filesystem::path& fragmentShaderPath);

    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& horizonColour,
                const glm::vec3& zenithColour,
                const glm::vec3& cloudColour,
                float elapsedTime);

    std::size_t getVertexCount() const { return vertexCount; }
    std::size_t getIndexCount() const { return indexCount; }

private:
    void releaseGraphicsResources();

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;

    Shader skyShader;
    bool shaderReady = false;
    GLint uniformView = -1;
    GLint uniformProjection = -1;
    GLint uniformHorizonColour = -1;
    GLint uniformZenithColour = -1;
    GLint uniformCloudColour = -1;
    GLint uniformTime = -1;
};
