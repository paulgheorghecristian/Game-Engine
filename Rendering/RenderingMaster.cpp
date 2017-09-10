#include "RenderingMaster.h"

RenderingMaster *RenderingMaster::m_instance = NULL;
Display *RenderingMaster::display;
Camera *RenderingMaster::camera;
glm::mat4 RenderingMaster::projectionMatrix;

RenderingMaster::RenderingMaster(Display *display,
                                 Camera *camera,
                                 glm::mat4 projectionMatrix) {
    RenderingMaster::display = display;
    RenderingMaster::camera = camera;
    RenderingMaster::projectionMatrix = projectionMatrix;
}

RenderingMaster::~RenderingMaster() {
    display->close();
    delete display;
    delete camera;
}

void RenderingMaster::init(Display *display,
                           Camera *camera,
                           glm::mat4 projectionMatrix) {
    if (m_instance == NULL) {
        m_instance = new RenderingMaster (display, camera, projectionMatrix);
    }
}

RenderingMaster *RenderingMaster::getInstance() {
    return m_instance;
}

void RenderingMaster::destroy () {
    if (m_instance != NULL) {
        delete m_instance;
    }
}

void RenderingMaster::clearScreen(float r, float g, float b, float a) {
    display->clear (r, g, b, a);
}

void RenderingMaster::swapBuffers() {
    display->swapBuffers ();
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

Camera *RenderingMaster::getCamera() {
    return camera;
}

glm::mat4 &RenderingMaster::getProjectionMatrix() {
    return projectionMatrix;
}
