#include "PostProcess.h"
#include <utility>

Mesh *PostProcess::renderingQuad = NULL;

void PostProcess::init (int width, int height, std::string &&shaderJsonFile) {
    bool result = true;
    int textureSamplerUniform = 0;

    this->width = width;
    this->height = height;

    float pixelWidth = 1.0f / width;
    float pixelHeight = 1.0f / height;

    postProcessShader.construct (std::forward<std::string>(shaderJsonFile));
    result &= postProcessShader.updateUniform ("pixelWidth", pixelWidth);
    result &= postProcessShader.updateUniform ("pixelHeight", pixelHeight);
    result &= postProcessShader.updateUniform ("textureSampler", textureSamplerUniform);

    if (renderingQuad == NULL) {
        renderingQuad = Mesh::getRectangle();
    }

    assert (result);
}

PostProcess::PostProcess (int width, int height, std::string &&shaderJsonFile) :  frameBuffer (width, height, 2),
                                                                                  inputTexture (frameBuffer.getRenderTargets()[0], 0),
                                                                                  resultingTexture (frameBuffer.getRenderTargets()[frameBuffer.getNumOfRenderTargets() - 1],
                                                                                                    5){ //todo wtf 5?
    init (width, height, std::forward<std::string> (shaderJsonFile));
}
PostProcess::PostProcess(int width, int height, GLuint inputTextureId, std::string &&shaderJsonFile) : frameBuffer (width, height, 1),
                                                                                                       inputTexture (inputTextureId, 0),
                                                                                                       resultingTexture (frameBuffer.getRenderTargets()[frameBuffer.getNumOfRenderTargets() - 1],
                                                                                                                        5) {//todo wtf 5?
    init (width, height, std::forward<std::string> (shaderJsonFile));
}

void PostProcess::bind() {
    /* call this before calling rendering functions */

    if (frameBuffer.getNumOfRenderTargets() > 1) {
        frameBuffer.bindSingleRenderTarget (0);
    }
}

void PostProcess::process(std::initializer_list<Texture *> txtList) {
    /* call this after calling rendering functs */

    if (frameBuffer.getNumOfRenderTargets() > 1) {
        frameBuffer.unbind();
    }

    frameBuffer.bindSingleRenderTarget (frameBuffer.getNumOfRenderTargets() - 1);

    postProcessShader.bind();
    inputTexture.use();
    for(auto txt : txtList) {
        txt->use();
    }
    renderingQuad->draw();

    frameBuffer.unbind();
    postProcessShader.unbind();
}

Texture &PostProcess::getResultingTexture() {
    return resultingTexture;
}

GLuint PostProcess::getResultingTextureId() {
    return frameBuffer.getRenderTargets()[frameBuffer.getNumOfRenderTargets() - 1];
}

GLuint PostProcess::getInputTextureId() {
    return inputTexture.getTextureId();
}

GLuint PostProcess::getFrameBufferObject() {
    return frameBuffer.getFrameBufferObject();
}

Shader &PostProcess::getShader() {
    return postProcessShader;
}
