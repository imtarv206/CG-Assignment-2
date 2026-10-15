#include "ShadowMap.h"

#include <iostream>

ShadowMap::~ShadowMap()
{
    release();
}

bool ShadowMap::initialise(GLsizei width, GLsizei height)
{
    release();
    mapWidth = width;
    mapHeight = height;

    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT32F,
        mapWidth,
        mapHeight,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float borderColour[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColour);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        depthTexture,
        0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const bool complete =
        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!complete)
    {
        std::cerr << "Shadow-map framebuffer is incomplete.\n";
        release();
    }

    return complete;
}

void ShadowMap::beginDepthPass() const
{
    glViewport(0, 0, mapWidth, mapHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::endDepthPass(GLsizei viewportWidth, GLsizei viewportHeight) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void ShadowMap::bindTexture(GLenum textureUnit) const
{
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
}

void ShadowMap::release()
{
    if (depthTexture != 0)
    {
        glDeleteTextures(1, &depthTexture);
        depthTexture = 0;
    }
    if (framebuffer != 0)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
    mapWidth = 0;
    mapHeight = 0;
}
