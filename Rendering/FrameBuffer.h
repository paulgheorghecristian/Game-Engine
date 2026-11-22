#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <vector>
#include <GL/glew.h>
#include <iostream>
#include <cassert>
#include "ErrorUtils.h"
//TODO add texture
class FrameBuffer
{
    public:
        FrameBuffer(int width, int height, unsigned int numOfRenderTargets, bool depthForVariance = false,
                    GLenum colorFormat = GL_RGBA8);
        FrameBuffer();
        ~FrameBuffer();

        ErrorCode init(int width, int height, unsigned int numOfRenderTargets, bool depthForVariance = false,
                       GLenum colorFormat = GL_RGBA8);

        void bindAllRenderTargets();
        void bindSingleRenderTarget (unsigned int index);
        void unbind();

        unsigned int getNumOfRenderTargets();
        inline int getWidth() const { return width; }
        inline int getHeight() const { return height; }
        GLuint getDepthTextureId();
        GLuint getFrameBufferObject();
        const std::vector<GLuint> &getRenderTargets();

    protected:
    private:
        int width, height;
        GLuint frameBufferId, depthBufferId, colorBufferId;
        GLint previousViewport[4];
        std::vector<GLuint> renderTargets;
        unsigned int numOfRenderTargets;

        bool m_depthForVariance;
};

#endif // FRAMEBUFFER_H
