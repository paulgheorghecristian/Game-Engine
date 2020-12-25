#include "FrameBuffer.h"

FrameBuffer::FrameBuffer() : width(-1), height(-1), numOfRenderTargets(-1)
{
}

FrameBuffer::FrameBuffer(int width, int height, unsigned int numOfRenderTargets, bool depthForVariance)
{
    ErrorCode ec;

    ec = init(width, height, numOfRenderTargets, depthForVariance);
    assert(ec == NO_ERROR);
}

ErrorCode FrameBuffer::init(int width, int height, unsigned int numOfRenderTargets, bool depthForVariance)
{
    glGetIntegerv (GL_VIEWPORT, previousViewport);

    this->width = width;
    this->height = height;
    this->numOfRenderTargets = numOfRenderTargets;
    this->renderTargets.resize(numOfRenderTargets);
    m_depthForVariance = depthForVariance;

    //generare framebuffer
    glGenFramebuffers(1, &frameBufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);

    if (numOfRenderTargets > 0) {
        glGenTextures(numOfRenderTargets, &renderTargets[0]);
    }
    for (unsigned int i = 0; i < numOfRenderTargets; i++) {
        glBindTexture (GL_TEXTURE_2D, renderTargets[i]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }

    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glGenTextures (1, &depthBufferId);
    glBindTexture(GL_TEXTURE_2D, depthBufferId);

    if (depthForVariance == false) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,  GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    } else {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F , width, height, 0, GL_RG, GL_FLOAT, 0);
    }

    glGenTextures(1, &stencilBufferId);
    glBindTexture(GL_TEXTURE_2D, stencilBufferId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, width, height, 0,  GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 0);

    //selecteaza attachment-ul pentru desenare color buffer
    for (unsigned int i = 0; i < numOfRenderTargets; i++) {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, renderTargets[i], 0);
    }

    if (depthForVariance == false) {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthBufferId, 0);
    } else {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + numOfRenderTargets, depthBufferId, 0);
    }
    glFramebufferTexture(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, stencilBufferId, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        print_error(OPENGL_ERROR, "Framebuffer is incomplete");
        return OPENGL_ERROR;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return NO_ERROR;
}


void FrameBuffer::bindSingleRenderTarget (unsigned int index)
{
    assert (index >= 0 && index < numOfRenderTargets);

    GLenum buffer[]= {GL_COLOR_ATTACHMENT0 + index};
#if 0
    /* very inefficient and I think
     * I don't even need it, inspect further
     */
    glGetIntegerv (GL_VIEWPORT, previousViewport);
#endif

    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferId);
    glDrawBuffers (1, buffer);
    glClearColor (0, 0, 0, 0);
    if (m_depthForVariance == false)
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
        glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (0, 0, width, height);
}

void FrameBuffer::bindAllRenderTargets()
{
    //bind-eaza acest framebuffer si deseneaza pe el, nu pe cel default
    #if 0
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    #endif

    glBindFramebuffer (GL_FRAMEBUFFER, frameBufferId);
    if (renderTargets.size() > 0) {
        glDrawBuffers (renderTargets.size(), &renderTargets[0]);
        glClearColor (0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    } else {
        if (m_depthForVariance == false) {
            glClear(GL_DEPTH_BUFFER_BIT);
        } else {
            GLenum buffer[]= {GL_COLOR_ATTACHMENT0 + numOfRenderTargets};
            glDrawBuffers(1, buffer);
            glClearColor(1, 1, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (0, 0, width, height);
}

void FrameBuffer::unbind() {
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
    glViewport (previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
}

unsigned int FrameBuffer::getNumOfRenderTargets()
{
    return renderTargets.size();
}

const std::vector<GLuint> &FrameBuffer::getRenderTargets()
{
    return renderTargets;
}

GLuint FrameBuffer::getDepthTextureId()
{
    return depthBufferId;
}

GLuint FrameBuffer::getFrameBufferObject()
{
    return frameBufferId;
}

FrameBuffer::~FrameBuffer()
{
    unbind ();
    glDeleteFramebuffers (1, &frameBufferId);
}
