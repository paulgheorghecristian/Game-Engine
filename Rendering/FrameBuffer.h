#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <vector>
#include <GL/glew.h>
#include <iostream>
#include <cassert>

class FrameBuffer
{
    public:
        FrameBuffer (int width, int height, unsigned int numOfRenderTargets);
        void bindAllRenderTargets();
        void bindSingleRenderTarget (unsigned int index);
        void unbind();
        unsigned int getNumOfRenderTargets();
        GLuint getDepthTextureId();
        GLuint getFrameBufferObject();
        const std::vector<GLuint> &getRenderTargets();
        virtual ~FrameBuffer();
    protected:
    private:
        int width, height;
        GLuint frameBufferId, depthBufferId, colorBufferId;
        GLint previousViewport[4];
        std::vector<GLuint> renderTargets;
        unsigned int numOfRenderTargets;
};

#endif // FRAMEBUFFER_H
