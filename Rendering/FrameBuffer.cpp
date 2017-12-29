#include "FrameBuffer.h"

FrameBuffer::FrameBuffer (int width, int height, unsigned int numOfRenderTargets) : width (width),
                                                                                    height (height),
                                                                                    numOfRenderTargets (numOfRenderTargets),
                                                                                    renderTargets (numOfRenderTargets) {
    //generare framebuffer
    glGenFramebuffers (1, &frameBufferId);
    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferId);

    if (numOfRenderTargets > 0) {
        glGenTextures (numOfRenderTargets, &renderTargets[0]);
    }
    for (unsigned int i = 0; i < numOfRenderTargets; i++) {
        glBindTexture (GL_TEXTURE_2D, renderTargets[i]);
        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    }

    glGenTextures (1, &depthBufferId);
    glBindTexture (GL_TEXTURE_2D, depthBufferId);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);

    //selecteaza attachment-ul pentru desenare color buffer
    for (unsigned int i = 0; i < numOfRenderTargets; i++) {
        glFramebufferTexture (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, renderTargets[i], 0);
    }

    glFramebufferTexture (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBufferId, 0);

    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer isn't complete!" << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;
        exit(1);
    }

    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
}

void FrameBuffer::bindSingleRenderTarget (unsigned int index) {
    assert (index >= 0 && index < numOfRenderTargets);

    GLenum buffer[]= {GL_COLOR_ATTACHMENT0 + index};
    glGetIntegerv (GL_VIEWPORT, previousViewport);

    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferId);
    glDrawBuffers (1, buffer);
    glClearColor (0, 0, 0, 0);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (0, 0, width, height);
}

void FrameBuffer::bindAllRenderTargets() {
    //bind-eaza acest framebuffer si deseneaza pe el, nu pe cel default
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferId);
    if (renderTargets.size() > 0) {
        glDrawBuffers (renderTargets.size(), &renderTargets[0]);
        glClearColor (0, 0, 0, 0);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        glClear (GL_DEPTH_BUFFER_BIT);
    }

    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (0, 0, width, height);
}

void FrameBuffer::unbind() {
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

unsigned int FrameBuffer::getNumOfRenderTargets() {
    return renderTargets.size();
}

const std::vector<GLuint> &FrameBuffer::getRenderTargets() {
    return renderTargets;
}

GLuint FrameBuffer::getDepthTextureId() {
    return depthBufferId;
}

GLuint FrameBuffer::getFrameBufferObject() {
    return frameBufferId;
}

FrameBuffer::~FrameBuffer() {
    unbind ();
    glDeleteFramebuffers (1, &frameBufferId);
}
