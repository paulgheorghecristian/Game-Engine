#include "camera.h"

Camera::Camera(glm::vec3 position,
               float xRotation,
               float yRotation,
               float zRotation) : position(position),
                                xRotation(xRotation),
                                yRotation(yRotation),
                                zRotation(zRotation),
                                forward(0.0, 0.0, -1.0),
                                right(1.0, 0.0, 0.0)
{

}

Camera::~Camera()
{

}

glm::mat4 Camera::getViewMatrix() {
    glm::mat4 matrix(1.0f);

    matrix = glm::rotate(matrix, -xRotation, glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, -yRotation, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 negPos(-position.x, -position.y, -position.z);

    matrix = glm::translate(matrix, negPos);

    return matrix;
}

void Camera::moveUp(float dx){
    position.y += dx;
}

void Camera::moveSideways(float dx){
    computeForward();
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0, 1.0, 0.0)));
    position = position + right * dx;
}

void Camera::moveForward(float distance){
    computeForward();
    position = position + forward * distance;
}

void Camera::rotateY(float dx){
    yRotation += dx;
}
void Camera::rotateX(float dx){
    if (glm::degrees(xRotation) < -60 && dx < 0) {
        return;
    }
    if (glm::degrees(xRotation) > 85 && dx > 0) {
        return;
    }
    xRotation += dx;
}
void Camera::invertForward(){
    xRotation *= -1;
}

void Camera::computeForward(){
    float sinX = glm::sin(xRotation);
    float cosX = glm::cos(xRotation);
    float sinY = glm::sin(yRotation);
    float cosY = glm::cos(yRotation);

    forward.x = -cosX * sinY;
    forward.y = sinX;
    forward.z = -cosX * cosY;
}

glm::vec3 Camera::getForward(){
    computeForward();
    return forward;
}

glm::vec3 Camera::getRight(){
    float yRot = yRotation - glm::radians(90.0f);
    right.x = -glm::sin(yRot);
    right.y = 0;
    right.z = -glm::cos(yRot);

    right = glm::normalize(right);
    return right;
}

glm::vec3 Camera::getUp(){
    computeForward();
    return glm::normalize(glm::cross(forward, getRight()));
}

void Camera::setPosition(const glm::vec3& pos){
    position = pos;
}

glm::vec3 Camera::getPosition(){
    return position;
}

glm::vec3 Camera::getRotation(){
    return glm::vec3(xRotation, yRotation, zRotation);
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
