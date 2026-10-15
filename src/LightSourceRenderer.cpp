#include "LightSourceRenderer.h"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>

LightSourceRenderer::~LightSourceRenderer()
{
    releaseGraphicsResources();
}

bool LightSourceRenderer::initialise(
    const std::filesystem::path& vertexShaderPath,
    const std::filesystem::path& fragmentShaderPath)
{
    releaseGraphicsResources();

    // สองสามเหลี่ยมรวมเป็นแผ่นสี่เหลี่ยมหนึ่งแผ่นในพิกัด -0.5 ถึง 0.5
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

    glBindVertexArray(quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glowShader.CreateFromFiles(vertexShaderPath, fragmentShaderPath);
    uniformView = glowShader.GetUniformLocation("view");
    uniformProjection = glowShader.GetUniformLocation("projection");
    uniformCameraRight = glowShader.GetUniformLocation("cameraRight");
    uniformCameraUp = glowShader.GetUniformLocation("cameraUp");
    uniformLightPosition = glowShader.GetUniformLocation("lightPosition");
    uniformLightColour = glowShader.GetUniformLocation("lightColour");
    uniformLightSize = glowShader.GetUniformLocation("lightSize");

    graphicsReady =
        uniformView >= 0 && uniformProjection >= 0 &&
        uniformCameraRight >= 0 && uniformCameraUp >= 0 &&
        uniformLightPosition >= 0 && uniformLightColour >= 0 &&
        uniformLightSize >= 0;

    if (!graphicsReady)
    {
        std::cerr << "Light-source shader is missing one or more required uniforms.\n";
        releaseGraphicsResources();
    }

    return graphicsReady;
}

void LightSourceRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraRight,
    const glm::vec3& cameraUp,
    const glm::vec3& lightPosition,
    const glm::vec3& lightColour,
    float size)
{
    if (!graphicsReady)
        return;

    glowShader.UseShader();
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(uniformCameraRight, 1, glm::value_ptr(cameraRight));
    glUniform3fv(uniformCameraUp, 1, glm::value_ptr(cameraUp));
    glUniform3fv(uniformLightPosition, 1, glm::value_ptr(lightPosition));
    glUniform3fv(uniformLightColour, 1, glm::value_ptr(lightColour));
    glUniform1f(uniformLightSize, size);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    glBindVertexArray(quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void LightSourceRenderer::releaseGraphicsResources()
{
    graphicsReady = false;

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

    glowShader.ClearShader();
}
