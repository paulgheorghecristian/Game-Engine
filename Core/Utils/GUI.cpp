#include "GUI.h"

glm::mat4 GUI::projectionMatrix;
Shader GUI::guiShader;

GUI::GUI(const glm::vec2 position,
         const glm::vec2 scale,
         const glm::vec2 rotation) : transform (glm::vec3 (position, -10),
                                                glm::vec3 (rotation, 0),
                                                glm::vec3 (scale, 1));
{

}

void GUI::init(int width, int height) {
    bool result = true;

    projectionMatrix = glm::ortho (0.0f, width, 0.0f, height);
    guiShader.construct ("res/shaders/guiShader.json");

    result &= guiShader.updateUniform ("projectionMatrix", (void *) &projectionMatrix);

    assert (result);
}

void GUI::render() {

}

void GUI::update() {

}

GUI::~GUI()
{

}
