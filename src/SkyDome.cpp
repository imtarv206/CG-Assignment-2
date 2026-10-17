#include "SkyDome.h"

#include <cmath>
#include <iostream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

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
    constexpr float minimumElevation = -PI / 6.0f; // ต่ำกว่าขอบฟ้า 30 องศา
    const float elevationStep =
        (0.5f * PI - minimumElevation) / static_cast<float>(stackCount);

    // ยื่นวงล่างลงใต้เส้นขอบฟ้า เพื่อไม่ให้เห็นช่องว่างเมื่อกล้องก้มลง
    for (unsigned int stack = 0; stack <= stackCount; ++stack)
    {
        const float elevation =
            minimumElevation + static_cast<float>(stack) * elevationStep;
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
    uniformPanorama = skyShader.GetUniformLocation("skyPanorama");
    uniformPanoramaYaw = skyShader.GetUniformLocation("panoramaYaw");

    shaderReady =
        uniformView >= 0 && uniformProjection >= 0 &&
        uniformPanorama >= 0 && uniformPanoramaYaw >= 0;

    if (!shaderReady)
        std::cerr << "SkyDome shader is missing one or more required uniforms.\n";

    return shaderReady;
}

bool SkyDome::initialiseTexture(const std::filesystem::path& texturePath)
{
    if (panoramaTexture != 0)
    {
        glDeleteTextures(1, &panoramaTexture);
        panoramaTexture = 0;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    const std::string filename = texturePath.string();
    unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (pixels == nullptr)
    {
        std::cerr << "[SKYDOME] Cannot load panorama: " << filename
                  << " | " << stbi_failure_reason() << '\n';
        return false;
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB8;
    if (channels == 1)
    {
        format = GL_RED;
        internalFormat = GL_R8;
    }
    else if (channels == 2)
    {
        format = GL_RG;
        internalFormat = GL_RG8;
    }
    else if (channels == 4)
    {
        format = GL_RGBA;
        internalFormat = GL_RGBA8;
    }
    else if (channels != 3)
    {
        std::cerr << "[SKYDOME] Unsupported channel count: " << channels << '\n';
        stbi_image_free(pixels);
        return false;
    }

    glGenTextures(1, &panoramaTexture);
    glBindTexture(GL_TEXTURE_2D, panoramaTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    std::cout << "[SKYDOME] Panorama loaded: " << filename
              << " (" << width << 'x' << height << ")\n";
    return panoramaTexture != 0;
}

void SkyDome::render(const glm::mat4& view,
                     const glm::mat4& projection,
                     float panoramaYawRadians)
{
    if (!shaderReady || panoramaTexture == 0 || vao == 0 || indexCount == 0)
        return;

    skyShader.UseShader();
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, panoramaTexture);
    glUniform1i(uniformPanorama, 0);
    glUniform1f(uniformPanoramaYaw, panoramaYawRadians);

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
    glBindTexture(GL_TEXTURE_2D, 0);
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
    if (panoramaTexture != 0)
    {
        glDeleteTextures(1, &panoramaTexture);
        panoramaTexture = 0;
    }

    vertexCount = 0;
    indexCount = 0;

    uniformView = -1;
    uniformProjection = -1;
    uniformPanorama = -1;
    uniformPanoramaYaw = -1;
}
