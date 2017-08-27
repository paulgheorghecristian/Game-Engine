#include "Transform.h"
#include <iostream>
Transform::Transform(const glm::vec3 &position = glm::vec3(0),
                     const glm::quat &rotation = glm::quat(1, 0, 0, 0),
                     const glm::vec3 &scale = glm::vec3(1)) : position (position),
                                                              rotation (rotation),
                                                              scale (scale) {
    glm::mat4 T = glm::translate(glm::mat4(1.0), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0), scale);

    modelMatrix = T*R*S;
    modelMatrixNeedsRefresh = false;
}

Transform::Transform(const glm::vec3 &position = glm::vec3(0),
                     const glm::vec3 &rotation = glm::vec3(0),
                     const glm::vec3 &scale = glm::vec3(1)) : Transform(position, glm::quat(rotation), scale) {

}

Transform::Transform(Transform &other) {
    position = other.getPosition();
    rotation = other.getRotation();
    scale = other.getScale();

    modelMatrix = other.getModelMatrix();
}

Transform &Transform::operator=(Transform &other) {
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

const glm::vec3 &Transform::getScale() const {
    return scale;
}

const glm::mat4 &Transform::getModelMatrix() {
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

void Transform::setScale(const glm::vec3 &scale) {
    this->scale = scale;
    modelMatrixNeedsRefresh = true;
}

Transform Transform::interpolateWith(const Transform &other, float ratio) {
    Transform t(glm::vec3(0), glm::vec3(0), glm::vec3(0));

    if (ratio > 1) {
        ratio = 1;
    } else if (ratio < 0) {
        ratio = 0;
    }

    t.setPosition(this->getPosition() * ratio + other.getPosition() * (1.0f-ratio));
    t.setScale(this->getScale() * ratio + other.getScale() * (1.0f-ratio));
    t.setRotation(glm::mix(this->getRotation(), other.getRotation(), ratio));

    return t;
}

void Transform::refreshModelMatrix() {
    glm::mat4 T = glm::translate(glm::mat4(1.0), position);
    glm::mat4 R = glm::mat4_cast(rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0), scale);

    this->modelMatrix = T*R*S;
    this->modelMatrixNeedsRefresh = false;
}

Transform::~Transform() {

}
