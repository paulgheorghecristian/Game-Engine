#include "DebugRenderer.hpp"

Shader DebugRenderer::lineShader;
DebugRenderer *DebugRenderer::m_instance;

DebugRenderer::DebugRenderer() {
    glGenVertexArrays(1, &vaoHandleLines);
    glBindVertexArray(vaoHandleLines);

    glGenBuffers(1, &vboHandleLines);

    glBindVertexArray(0);
}

DebugRenderer::~DebugRenderer() {

}

DebugRenderer *DebugRenderer::getInstance() {
    return m_instance;
}

void DebugRenderer::init() {
    if (m_instance == NULL) {
        m_instance = new DebugRenderer();
    }
}

void DebugRenderer::destroy() {
    if (m_instance != NULL) {
        delete m_instance;
    }
}


void DebugRenderer::addLinePoint(const glm::vec3 &point) {
    m_linePoints.push_back(point);
}

void DebugRenderer::clearLinePoints() {
    m_linePoints.clear();
}

void DebugRenderer::render() {
    renderLines();
}


void DebugRenderer::renderLines() {
    if (m_linePoints.size() == 0)
        return;

    lineShader.bind();

    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(vaoHandleLines);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandleLines);
    glBufferData(GL_ARRAY_BUFFER,
                sizeof(glm::vec3) * m_linePoints.size(),
                &m_linePoints[0],
                GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *) 0);

    glLineWidth(5);
    glDrawArrays(GL_LINE_STRIP, 0, m_linePoints.size());

    glBindVertexArray(0);
    lineShader.unbind();

    glEnable(GL_DEPTH_TEST);
    glLineWidth(1);
}