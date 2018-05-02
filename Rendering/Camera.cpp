#include "camera.h"

Camera::Camera(glm::vec3 position,
               float xRotation,
               float yRotation,
               float zRotation) : position(position),
                                  rotation (xRotation, yRotation, zRotation),
                                  forward(0.0, 0.0, -1.0),
                                  right(1.0, 0.0, 0.0),
                                  viewMatrix (1.0f),
                                  forwardNeedsUpdate (true),
                                  viewMatrixNeedsUpdate (true) {

}

Camera::~Camera() {

}

const glm::mat4 &Camera::getViewMatrix() {
    if (viewMatrixNeedsUpdate) {
        computeViewMatrix ();
    }

    return viewMatrix;
}

void Camera::moveUp(float dx) {
    position.y += dx;

    viewMatrixNeedsUpdate = true;
}

void Camera::moveSideways(float dx) {
    if (forwardNeedsUpdate) {
        computeForward();
    }
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0, 1.0, 0.0)));
    position = position + right * dx;

    viewMatrixNeedsUpdate = true;
}

void Camera::moveForward(float distance) {
    if (forwardNeedsUpdate) {
        computeForward();
    }
    position = position + forward * distance;

    viewMatrixNeedsUpdate = true;
}

void Camera::rotateY(float dx) {
    rotation.y += dx;

    forwardNeedsUpdate = true;
    viewMatrixNeedsUpdate = true;
}

void Camera::rotateX(float dx) {
    if (glm::degrees(rotation.x) < -60 && dx < 0) {
        return;
    }
    if (glm::degrees(rotation.x) > 85 && dx > 0) {
        return;
    }
    rotation.x += dx;

    forwardNeedsUpdate = true;
    viewMatrixNeedsUpdate = true;
}

void Camera::invertForward() {
    rotation.x *= -1;

    viewMatrixNeedsUpdate = true;
    forwardNeedsUpdate = true;
}

void Camera::computeForward() {
    assert (forwardNeedsUpdate);

    float sinX = glm::sin(rotation.x);
    float cosX = glm::cos(rotation.x);
    float sinY = glm::sin(rotation.y);
    float cosY = glm::cos(rotation.y);

    forward.x = -cosX * sinY;
    forward.y = sinX;
    forward.z = -cosX * cosY;

    forwardNeedsUpdate = false;
}

void Camera::computeViewMatrix () {
    assert (viewMatrixNeedsUpdate);

    glm::quat xRotQuat = glm::angleAxis (-rotation.x, glm::vec3(1,0,0));
    glm::quat yRotQuat = glm::angleAxis (-rotation.y, glm::vec3(0,1,0));

    glm::quat orientation = xRotQuat * yRotQuat;
    orientation = glm::normalize (orientation);

    viewMatrix = glm::mat4_cast (orientation);
    viewMatrix = glm::translate (viewMatrix, -position);

    viewMatrixNeedsUpdate = false;
}

const glm::vec3 &Camera::getForward() {
    if (forwardNeedsUpdate) {
        computeForward ();
    }
    return forward;
}

const glm::vec3 &Camera::getRight() {
    float yRot = rotation.y - glm::radians (90.0f);

    right.x = -glm::sin (yRot);
    right.y = 0;
    right.z = -glm::cos (yRot);

    right = glm::normalize (right);

    return right;
}

glm::vec3 Camera::getUp() {
    if (forwardNeedsUpdate) {
        computeForward ();
    }
    return glm::normalize (glm::cross (getRight (), forward));
}

void Camera::setPosition(const glm::vec3& pos) {
    position = pos;

    viewMatrixNeedsUpdate = true;
}

const glm::vec3 &Camera::getPosition() {
    return position;
}

const glm::vec3 &Camera::getRotation() {
    return rotation;
}

void Camera::setRotation (float xRot, float yRot, float zRot) {
    rotation.x = xRot;
    rotation.y = yRot;
    rotation.z = zRot;

    viewMatrixNeedsUpdate = true;
    forwardNeedsUpdate = true;
}

void Camera::setRotation (const glm::vec3 &rotation) {
    this->rotation = rotation;

    viewMatrixNeedsUpdate = true;
    forwardNeedsUpdate = true;
}

/* contains the y rotation also
void Camera::compute_forward(){
    float sinX = glm::sin(xRotation);
    float cosX = glm::cos(xRotation);
    float sinY = glm::sin(yRotation);
    float cosY = glm::cos(yRotation);

    forward.x = -cosX * sinY;
    forward.y = sinX;
    forward.z = -cosX * cosY;

    forward = glm::normalize(forward);
}*/
