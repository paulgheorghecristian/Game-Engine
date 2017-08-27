#include "RenderingMaster.h"

RenderingMaster::RenderingMaster(Display *display,
                                 Camera *camera,
                                 glm::mat4 projectionMatrix,
                                 std::vector<Entity *> &entities) : display (display),
                                                                     camera (camera),
                                                                     projectionMatrix (projectionMatrix),
                                                                     entities (entities) {
    glm::mat4 viewMatrix = camera->getViewMatrix();
    bool result = true;

    for (auto const &entity : entities) {
        Component *component;
        if ((component = entity->getComponent(Entity::Flags::RENDERABLE)) != NULL) {
            RenderComponent * renderComponent = (RenderComponent *) component;
            if(std::find(shaders.begin(), shaders.end(), renderComponent->getShader()) == shaders.end()) {
                shaders.push_back (renderComponent->getShader());
            }
        }
    }

    for (auto const &shader : shaders) {
        result &= shader->updateUniform ("projectionMatrix", (void *) &projectionMatrix);
        result &= shader->updateUniform ("viewMatrix", (void *) &viewMatrix);
    }

    assert (result);

}

RenderingMaster::~RenderingMaster() {
    display->close();
    for (auto const &shader : shaders) {
        delete shader;
    }
    delete display;
    delete camera;
}

void RenderingMaster::clearScreen(float r, float g, float b, float a) {
    display->clear (r, g, b, a);
}

void RenderingMaster::swapBuffers() {
    display->swapBuffers ();
}

void RenderingMaster::render() {
    clearScreen(1, 1, 1, 1);

    for (auto entity : entities) {
        entity->render();
    }

    swapBuffers();
}

void RenderingMaster::update() {
    bool result = true;
    glm::mat4 viewMatrix = camera->getViewMatrix();

    for (auto shader : shaders) {
        result &= shader->updateUniform ("viewMatrix", (void *) &viewMatrix);
    }

    assert (result);
}

void RenderingMaster::moveCameraForward(float distance) {
    camera->moveForward(distance);
}

void RenderingMaster::moveCameraSideways(float distance) {
    camera->moveSideways(distance);
}

void RenderingMaster::rotateCameraX(float rotX) {
    camera->rotateX(rotX);
}
void RenderingMaster::rotateCameraY(float rotY) {
    camera->rotateY(rotY);
}

Display *RenderingMaster::getDisplay() {
    return display;
}
