#ifndef GBUFFER_H
#define GBUFFER_H

#include <GL/glew.h>
#include <iostream>

class GBuffer
{
    public:
        GBuffer();
        void generate(unsigned int width, unsigned int height);
        void destroy();
        GLuint getColorTexture();
        GLuint getPositionTexture();
        GLuint getNormalTexture();
        GLuint getRoughnessTexture();
        GLuint getLightAccumulationTexture();
        GLuint getDepthTexture();
        GLuint getFrameBufferObject();
        void bindForScene();
        void bindForLights();
        void bindForStencil();
        void unbind();
        unsigned int getWidth();
        unsigned int getHeight();
        virtual ~GBuffer();
    protected:
    private:
        unsigned int width, height;
        GLuint frameBufferObject;
        GLuint textureNormal;
        GLuint textureColor;
        GLuint textureLightAccumulation;
        GLuint textureDepth;
        GLuint textureRoughness;
        GLuint textureStencil;
};

#endif // GBUFFER_H
