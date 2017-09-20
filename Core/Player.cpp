#include "Player.h"

Player::Player (Camera *camera, Transform &transform) : Entity (transform) {
    addComponent (new FirstPersonComponent (camera));
}

Player::FirstPersonComponent::FirstPersonComponent(Camera *camera) : camera (camera) {

}

const unsigned int Player::FirstPersonComponent::getFlag() const {
    return Entity::Flags::THIRD_PERSON_CAMERA;
}

void Player::FirstPersonComponent::input() {

}

void Player::FirstPersonComponent::update() {
    glm::vec3 pos = _entity->getTransform().getPosition();
    pos.y += NECK_HEIGHT;
    camera->setPosition (pos);
}

void Player::FirstPersonComponent::render() {

}

void Player::FirstPersonComponent::init() {

}

Player::~Player () {
    //dtor
}
