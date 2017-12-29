#include "Player.h"

Player::Player (Camera *camera, Transform &transform) {
    this->transform = transform;
    addComponent (new FirstPersonComponent (camera));
}

Player::FirstPersonComponent::FirstPersonComponent(Camera *camera) : camera (camera) {

}

const unsigned int Player::FirstPersonComponent::getFlag() const {
    return Entity::Flags::THIRD_PERSON_CAMERA;
}

void Player::FirstPersonComponent::input(Input &inputManager) {
    PhysicsComponent *physicsComponent = dynamic_cast<PhysicsComponent *> (_entity->getComponent (Entity::Flags::DYNAMIC));

    if (physicsComponent != NULL) {
        glm::vec3 forward = camera->getForward();
        glm::vec3 right = camera->getRight();
        glm::vec3 vel(0);

        if (inputManager.getKey(SDLK_w)) {
            vel += glm::vec3 (forward.x, 0, forward.z);
        } else if (inputManager.getKey(SDLK_s)) {
            vel -= glm::vec3 (forward.x, 0, forward.z);
        }

        if (inputManager.getKey(SDLK_d)) {
            vel += glm::vec3 (right.x, 0, right.z);
        } else if (inputManager.getKey(SDLK_a)) {
            vel -= glm::vec3 (right.x, 0, right.z);
        }

        if (glm::length (vel) != 0) {
            vel = glm::normalize(vel);
        }
        btVector3 velBt(vel.x, 0, vel.z);
        velBt *= PLAYER_SPEED;
        physicsComponent->getRigidBody()->applyCentralForce(velBt);
    }
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
