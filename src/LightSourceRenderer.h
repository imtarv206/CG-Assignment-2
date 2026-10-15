#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <filesystem>

#include "Libs/Shader.h"

// วาดแหล่งกำเนิดแสงเป็น billboard เรืองแสงที่หันเข้าหากล้องเสมอ
class LightSourceRenderer
{
public:
    LightSourceRenderer() = default;
    ~LightSourceRenderer();

    LightSourceRenderer(const LightSourceRenderer&) = delete;
    LightSourceRenderer& operator=(const LightSourceRenderer&) = delete;

    bool initialise(const std::filesystem::path& vertexShaderPath,
                    const std::filesystem::path& fragmentShaderPath);

    void render(const glm::mat4& view,
                const glm::mat4& projection,
                const glm::vec3& cameraRight,
                const glm::vec3& cameraUp,
                const glm::vec3& lightPosition,
                const glm::vec3& lightColour,
                float size);

private:
    void releaseGraphicsResources();

    Shader glowShader;
    GLuint quadVao = 0;
    GLuint quadVbo = 0;
    bool graphicsReady = false;

    GLint uniformView = -1;
    GLint uniformProjection = -1;
    GLint uniformCameraRight = -1;
    GLint uniformCameraUp = -1;
    GLint uniformLightPosition = -1;
    GLint uniformLightColour = -1;
    GLint uniformLightSize = -1;
};
