#pragma once

#include <GL/glew.h>

// เป็นเจ้าของ framebuffer และ depth texture ที่ใช้ใน shadow pass
class ShadowMap
{
public:
    ShadowMap() = default;
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    bool initialise(GLsizei width, GLsizei height);
    void beginDepthPass() const;
    void endDepthPass(GLsizei viewportWidth, GLsizei viewportHeight) const;
    void bindTexture(GLenum textureUnit) const;

private:
    void release();

    GLuint framebuffer = 0;
    GLuint depthTexture = 0;
    GLsizei mapWidth = 0;
    GLsizei mapHeight = 0;
};
