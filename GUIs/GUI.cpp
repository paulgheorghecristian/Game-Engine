#include "GUI.h"

#include "BarGUI.h"
#include "Shader.h"

glm::mat4 GUI::projectionMatrix;

GUI::GUI(const glm::vec2 &position,
         const glm::vec2 &scale,
         const glm::vec2 &rotation) : m_transform (glm::vec3 (position, -10),
                                                   glm::vec3 (rotation, 0),
                                                   glm::vec3 (scale, 0))
{
}

void GUI::init (int width, int height) {
    bool result = true;
    projectionMatrix = glm::ortho (0.0f, (float) width, 0.0f, (float) height, 1.0f, 1000.0f);

    result &= BarGUI::getBarGUIShader().updateUniform("projectionMatrix", (void *) &GUI::projectionMatrix);

    assert (result);
}

const Transform &GUI::getTransform () const {
    return m_transform;
}

GUI::~GUI()
{
    //dtor
}
