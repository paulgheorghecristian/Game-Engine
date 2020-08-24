#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include "FrameBuffer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include <string>

class PostProcess
{
    public:
        PostProcess(int width, int height, std::string &&shaderJsonFile);
        PostProcess(int width, int height, GLuint inputTextureId, std::string &&shaderJsonFile);

        void bind();
        void process(std::initializer_list<Texture *> list = {});
        GLuint getResultingTextureId();
        GLuint getFrameBufferObject();
        GLuint getInputTextureId();

        Texture &getResultingTexture();
        Shader &getShader();
    protected:
    private:
        void init (int width, int height, std::string &&shaderJsonFile);

        int width, height;

        FrameBuffer frameBuffer;
        Shader postProcessShader;
        Texture inputTexture;
        Texture resultingTexture;

        static Mesh *renderingQuad;

};

#endif // POSTPROCESS_H
