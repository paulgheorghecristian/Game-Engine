#include "Transform.h"
#include <iostream>
Transform::Transform(const glm::vec3 &position,
                     const glm::quat &rotation,
                     const glm::vec3 &scale) : position (position),
                                               rotation (rotation),
                                               scale (scale) {
    glm::mat4 T = glm::translate(glm::mat4(1.0), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0), scale);

    modelMatrix = T*R*S;
    modelMatrixNeedsRefresh = false;
}

Transform::Transform(const glm::vec3 &position,
                     const glm::vec3 &rotation,
                     const glm::vec3 &scale) : Transform (position, glm::quat (glm::radians(rotation)), scale) {}

Transform::Transform() : Transform (glm::vec3(0), glm::vec3 (0), glm::vec3(1)) {}

Transform::Transform(const Transform &other) {
    position = other.getPosition();
    rotation = other.getRotation();
    scale = other.getScale();

    modelMatrix = other.getModelMatrix();
}

Transform &Transform::operator=(const Transform &other) {
    position = other.getPosition();
    rotation = other.getRotation();
    scale = other.getScale();

    modelMatrix = other.getModelMatrix();
    return *(this);
}

const glm::vec3 &Transform::getPosition() const {
    return position;
}

const glm::quat &Transform::getRotation() const {
    return rotation;
}

const glm::vec3 Transform::getEulerRotation() const {
    return glm::eulerAngles(rotation) * 3.14159f / 180.f;
}

const glm::vec3 &Transform::getScale() const {
    return scale;
}

const glm::mat4 &Transform::getModelMatrix() const {
    if (modelMatrixNeedsRefresh) {
        refreshModelMatrix();
    }

    return modelMatrix;
}

void Transform::setPosition(const glm::vec3 &position) {
    this->position = position;
    modelMatrixNeedsRefresh = true;
}

void Transform::setRotation(const glm::quat &rotation) {
    this->rotation = rotation;
    modelMatrixNeedsRefresh = true;
}

void Transform::setRotation(const glm::vec3 &rotation) {
    this->rotation = glm::quat(rotation);
    modelMatrixNeedsRefresh = true;
}

void Transform::setScale(const glm::vec3 &scale) {
    this->scale = scale;
    modelMatrixNeedsRefresh = true;
}

void Transform::setModelMatrix (const glm::mat4 &modelMatrix) {
    this->modelMatrix = modelMatrix;
    modelMatrixNeedsRefresh = false;
}

void Transform::addPosition(float x, float y, float z) {
    if (!x && !y && !z) {
        return;
    }

    position.x += x;
    position.y += y;
    position.z += z;

    modelMatrixNeedsRefresh = true;
}

void Transform::addRotation(float x, float y, float z) {
    if (!x && !y && !z) {
        return;
    }

    rotation = glm::normalize(rotation * glm::quat(glm::vec3(x, y, z)));

    modelMatrixNeedsRefresh = true;
}

void Transform::addScale(float x, float y, float z) {
    scale.x += x;
    scale.y += y;
    scale.z += z;

    if (!x && !y && !z) {
        return;
    }

    modelMatrixNeedsRefresh = true;
}

void Transform::interpolateWith(const Transform &other, float ratio) {

    if (ratio > 1) {
        ratio = 1;
    } else if (ratio < 0) {
        ratio = 0;
    }

    ratio = 1-ratio;

    this->setPosition(glm::mix(this->getPosition(), other.getPosition(), ratio));
    this->setScale(glm::mix(this->getScale(), other.getScale(), ratio));
    this->setRotation(glm::slerp(this->getRotation(), other.getRotation(), ratio));

    modelMatrixNeedsRefresh = true;
}

void Transform::refreshModelMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0), scale);

    this->modelMatrix = T*R*S;
    this->modelMatrixNeedsRefresh = false;
}

Transform::~Transform() {

}
