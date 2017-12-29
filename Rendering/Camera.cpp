#include "camera.h"

Camera::Camera(glm::vec3 position,
               float xRotation,
               float yRotation,
               float zRotation) : position(position),
                                  rotation (xRotation, yRotation, zRotation),
                                  forward(0.0, 0.0, -1.0),
                                  right(1.0, 0.0, 0.0),
                                  viewMatrix (1.0f) {

}

Camera::~Camera() {

}

const glm::mat4 &Camera::getViewMatrix() {
    glm::quat xRotQuat = glm::angleAxis (-rotation.x, glm::vec3(1,0,0));
    glm::quat yRotQuat = glm::angleAxis (-rotation.y, glm::vec3(0,1,0));

    glm::quat orientation = xRotQuat * yRotQuat;
    orientation = glm::normalize (orientation);

    viewMatrix = glm::mat4_cast (orientation);
    viewMatrix = glm::translate (viewMatrix, -position);

    return viewMatrix;
}

void Camera::moveUp(float dx) {
    position.y += dx;
}

void Camera::moveSideways(float dx) {
    computeForward();
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0, 1.0, 0.0)));
    position = position + right * dx;
}

void Camera::moveForward(float distance) {
    computeForward();
    position = position + forward * distance;
}

void Camera::rotateY(float dx) {
    rotation.y += dx;
}

void Camera::rotateX(float dx) {
    if (glm::degrees(rotation.x) < -60 && dx < 0) {
        return;
    }
    if (glm::degrees(rotation.x) > 85 && dx > 0) {
        return;
    }
    rotation.x += dx;
}

void Camera::invertForward() {
    rotation.x *= -1;
}

void Camera::computeForward() {
    float sinX = glm::sin(rotation.x);
    float cosX = glm::cos(rotation.x);
    float sinY = glm::sin(rotation.y);
    float cosY = glm::cos(rotation.y);

    forward.x = -cosX * sinY;
    forward.y = sinX;
    forward.z = -cosX * cosY;
}

const glm::vec3 &Camera::getForward() {
    computeForward();
    return forward;
}

const glm::vec3 &Camera::getRight() {
    float yRot = rotation.y - glm::radians(90.0f);
    right.x = -glm::sin(yRot);
    right.y = 0;
    right.z = -glm::cos(yRot);

    right = glm::normalize(right);
    return right;
}

glm::vec3 Camera::getUp() {
    computeForward();
    return glm::normalize(glm::cross(forward, getRight()));
}

void Camera::setPosition(const glm::vec3& pos) {
    position = pos;
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
}

void Camera::setRotation (const glm::vec3 &rotation) {
    this->rotation = rotation;
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
