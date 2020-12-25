#include "GBuffer.h"

GBuffer::GBuffer () {

}

void GBuffer::generate (unsigned int width, unsigned int height){
    this->width = width;
    this->height = height;

    //genereaza un obiect de tip framebuffer si apoi leaga-l la pipeline
    glGenFramebuffers(1, &frameBufferObject);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject);

    //albedo texture
    glGenTextures(1, &textureColor);
    glBindTexture(GL_TEXTURE_2D, textureColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    //normal texture
    glGenTextures(1, &textureNormal);
    glBindTexture(GL_TEXTURE_2D, textureNormal);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, width, height, 0, GL_RGB, GL_FLOAT, 0);

    //material roughness
    glGenTextures(1, &textureRoughness);
    glBindTexture(GL_TEXTURE_2D, textureRoughness);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    //light accumulation texture
    glGenTextures(1, &textureLightAccumulation);
    glBindTexture(GL_TEXTURE_2D, textureLightAccumulation);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

    //genereaza textura de adancime, format DEPTH (un singur canal), FARA date, fara filtrare
    glGenTextures(1, &textureDepth);
    glBindTexture(GL_TEXTURE_2D, textureDepth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,  GL_DEPTH_COMPONENT, GL_FLOAT, 0);

    glGenTextures(1, &textureStencil);
    glBindTexture(GL_TEXTURE_2D, textureStencil);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, width, height, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 0);

    //leaga texturi la framebuffer , 0 de la sfarsit se refera la ce nivel din mipmap, 0 fiind cel mai de sus/mare.
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+0, textureColor, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+1, textureNormal, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+2, textureRoughness, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+3, textureLightAccumulation, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureDepth, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, textureStencil, 0);

    //verifica stare
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "EROARE!!! Framebuffer-ul nu este complet." << std::endl;
        std::terminate();
    }

    //nu sunt legat la pipeline
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GBuffer::destroy(){
    glDeleteTextures(1, &textureNormal);
    glDeleteTextures(1, &textureColor);
    glDeleteTextures(1, &textureLightAccumulation);
    glDeleteTextures(1, &textureDepth);
    glDeleteTextures(1, &textureRoughness);
    glDeleteTextures(1, &textureStencil);
    glDeleteFramebuffers(1, &frameBufferObject);
}

GLuint GBuffer::getColorTexture(){
    return textureColor;
}

GLuint GBuffer::getNormalTexture(){
    return textureNormal;
}

GLuint GBuffer::getLightAccumulationTexture(){
    return textureLightAccumulation;
}

GLuint GBuffer::getDepthTexture(){
    return textureDepth;
}

GLuint GBuffer::getRoughnessTexture() {
    return textureRoughness;
}

void GBuffer::bindForScene(){
    GLenum buffersGeometry[] = {GL_COLOR_ATTACHMENT0,
                                GL_COLOR_ATTACHMENT1,
                                GL_COLOR_ATTACHMENT2};

    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferObject);
    glDrawBuffers(3, buffersGeometry);
}

void GBuffer::bindForLights() {
    GLenum buffersGeometry[] = {GL_COLOR_ATTACHMENT3};

    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject);
    glDrawBuffers(1, buffersGeometry);
}

void GBuffer::bindForStencil() {
    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject);
    glDrawBuffer(GL_NONE);
}

void GBuffer::unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

unsigned int GBuffer::getWidth() {
    return width;
}

unsigned int GBuffer::getHeight() {
    return height;
}

GLuint GBuffer::getFrameBufferObject() {
    return frameBufferObject;
}

GBuffer::~GBuffer()
{
    destroy();
}
