#include "Player.h"

#include "RenderingMaster.h"
#include "PhysicsMaster.h"

#include "Common.h"

#define NECK_HEIGHT 10.0f
#define PLAYER_WALK_SPEED 100.0f
#define PLAYER_RUN_SPEED 200.0f
#define DOWN_LENGTH 1000.0f
#define JUMP_SPEED 5600.0f
#define EPS 0.003

#define STEP_FRQ 0.16
#define STEP_AMP 0.35
#define STEP_ROT_FRQ 0.5
#define STEP_ROT_AMP 0.005

Player::Player (Transform &transform) {
    this->m_transform = transform;
    addComponent (new FirstPersonComponent ());
    addComponent (new PlayerControllerComponent ());
}

std::string Player::jsonify() {
    return std::string("");
}

Player::FirstPersonComponent::FirstPersonComponent() {
    m_camera = RenderingMaster::getInstance()->getCamera();
    sineStep = -glm::half_pi<float>();
    dir = true;
    animFOV = false;
    currFOV = 75.0f;
}

const unsigned int Player::FirstPersonComponent::getFlag() const {
    return Entity::Flags::THIRD_PERSON_CAMERA;
}

void Player::FirstPersonComponent::input(Input &inputManager) {
    Player::PlayerControllerComponent *playerController = 
                dynamic_cast<Player::PlayerControllerComponent *> (_entity->getComponent (Entity::Flags::PLAYER_CONTROLLER));

    if (playerController == NULL)
        return;

    float walked = glm::length(glm::vec3(lastPosDiff.x, 0.0f, lastPosDiff.z));

    if (inputManager.getKey(SDLK_LSHIFT) && walked >= 0.45f) {
        currFOV = 85.0f;
        RenderingMaster::getInstance()->setFOV(currFOV);
        playerController->speed = PLAYER_RUN_SPEED;
        animFOV = false;
    }
    if (inputManager.getKeyUp(SDLK_LSHIFT) || (currFOV > 75.0f && walked < 0.45f)) {
        animFOV = true;
        playerController->speed = PLAYER_WALK_SPEED;
    }

    if (animFOV == true) {
        currFOV -= 0.5f;
        if (currFOV <= 75.0f) {
            currFOV = 75.0f;
            animFOV = false;
        }
        RenderingMaster::getInstance()->setFOV(currFOV);

    }
}

void Player::FirstPersonComponent::update() {
    glm::vec3 pos = _entity->getTransform().getPosition();

    lastPosDiff = lastPos - pos;
    float step = (glm::length(glm::vec3(lastPosDiff.x, 0.0f, lastPosDiff.z)))*STEP_FRQ;

    sineStep += (dir == true) ? step : -step;
    if (sineStep > glm::half_pi<float>() && dir) {
        dir = false;
        sineStep = glm::half_pi<float>();
    } else if (sineStep < -glm::half_pi<float>()) {
        dir = true;
        sineStep = -glm::half_pi<float>();
    }

    pos.y += NECK_HEIGHT + glm::sin(sineStep)*STEP_AMP;

    m_camera->setPosition(pos);
    m_camera->setZRot(glm::sin(sineStep*STEP_ROT_FRQ)*STEP_ROT_AMP);

    lastPos = pos;
}

void Player::FirstPersonComponent::render() {

}

void Player::FirstPersonComponent::init() {

}

Player::PlayerControllerComponent::PlayerControllerComponent () : m_isJumping(false), m_wasSpaceReleased(true), m_freeRoam(false) {
    m_camera = RenderingMaster::getInstance()->getCamera();
    speed = PLAYER_WALK_SPEED;
}

void Player::PlayerControllerComponent::input(Input &inputManager) {
    glm::vec3 force(0);
    PhysicsComponent *physicsComponent = dynamic_cast<PhysicsComponent *> (_entity->getComponent (Entity::Flags::DYNAMIC));

    if (physicsComponent == NULL) {
        return;
    }

    const glm::vec3 &forward = m_camera->getForward();
    const glm::vec3 &right = m_camera->getRight();

    if (inputManager.getKeyDown(SDLK_f)) {
        m_freeRoam = !m_freeRoam;
    }

    if (inputManager.getKey(SDLK_w)) {
        force += glm::vec3 (forward.x, m_freeRoam ? forward.y : 0, forward.z);
    } else if (inputManager.getKey(SDLK_s)) {
        force -= glm::vec3 (forward.x, m_freeRoam ? forward.y : 0, forward.z);
    }

    if (inputManager.getKey(SDLK_d)) {
        force += glm::vec3 (right.x, m_freeRoam ? right.y : 0, right.z);
    } else if (inputManager.getKey(SDLK_a)) {
        force -= glm::vec3 (right.x, m_freeRoam ? right.y : 0, right.z);
    }

    if (!m_freeRoam) {
        if (inputManager.getKeyDown(SDLK_SPACE) && m_wasSpaceReleased) {
            if (!m_isJumping) {
                jump(physicsComponent);
            }
            m_wasSpaceReleased = false;
        }

        if (inputManager.getKeyUp(SDLK_SPACE)) {
            m_wasSpaceReleased = true;
        }
    }

    if (glm::length(force) != 0) {
        force = glm::normalize(force) * speed * (m_isJumping ? 0.8f : 1.0f);
        physicsComponent->getRigidBody()->applyCentralForce(btVector3(force.x, m_freeRoam ? force.y : 0, force.z));
    }
}

void Player::PlayerControllerComponent::update() {
    btRigidBody *m_body;
    glm::vec3 playerPosition = _entity->getTransform().getPosition();
    float closestHitFraction = 1000.0f;
    float values[5][2]={{0, 1.0},
                      {0, -1.0},
                      {1.0, 0},
                      {-1.0, 0},
                      {0,0}
                     };
    btVector3 normal, hitPos;
    PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);

    if (physicsComponent == NULL) {
        return;
    }
    m_body = physicsComponent->getRigidBody();
    float radius = physicsComponent->getBoundingBodyScale().x * 2.0f;

    if (!m_freeRoam) {
        m_body->setGravity(PhysicsMaster::getInstance()->getWorld()->getGravity());

        for (int i = 0; i < 5; i++){
            float xPlayerPosition = playerPosition.x+radius*values[i][0]*0.5;
            float zPlayerPosition = playerPosition.z+radius*values[i][1]*0.5;
            btVector3 from = btVector3(xPlayerPosition, playerPosition.y, zPlayerPosition);
            btVector3 to = btVector3(xPlayerPosition, -10000, zPlayerPosition);

            btCollisionWorld::ClosestRayResultCallback rayCallback(from, to);
            PhysicsMaster::getInstance()->getWorld()->rayTest(from, to, rayCallback);

            if (rayCallback.hasHit()) {
                UserData *data = (UserData *) rayCallback.m_collisionObject->getUserPointer();

                if (data != NULL &&
                    (data->type == PointerType::ENTITY ||
                    data->type == PointerType::LIGHT)) {
                    continue;
                }

                if (rayCallback.m_closestHitFraction < closestHitFraction){
                    closestHitFraction = rayCallback.m_closestHitFraction;
                    normal = rayCallback.m_hitNormalWorld;
                    hitPos = rayCallback.m_hitPointWorld;
                }
            }
        }

        float currentGroundDistance = glm::distance (glm::vec3(hitPos.x(), hitPos.y(), hitPos.z()), playerPosition);
        const btVector3 &currentVel = m_body->getLinearVelocity();

        /* 35 magic number, chosen by trial and error */
        if (!m_isJumping && currentGroundDistance < 35) {
            btTransform transform = m_body->getCenterOfMassTransform();
            btVector3 origin = transform.getOrigin();

            transform.setOrigin(btVector3(origin.x(), hitPos.y() + 31, origin.z()));
            m_body->setCenterOfMassTransform(transform);
            m_body->setLinearVelocity(btVector3(currentVel.x(), 0, currentVel.z()));
        }

        if (m_isJumping || currentVel.y() < -5.0) {
            m_body->setDamping(btScalar(0.2), btScalar(0));
        }

        /*36 magic number, chosen by trial and error */
        if(currentGroundDistance <= 36 && currentVel.y() <= 0){
            m_isJumping = false;

            float dot_prod = glm::dot(glm::vec3(0,1,0),
                                glm::vec3(normal.x(), normal.y(), normal.z()));

            m_body->setDamping(btScalar(0.4+(1.1-dot_prod)), btScalar(0));
            if (dot_prod < 0.7) {
                m_isJumping = true;
            }
        }
    } else {
        m_body->setGravity(btVector3(0, 0, 0));
    }
}

void Player::PlayerControllerComponent::render() {

}

const unsigned int Player::PlayerControllerComponent::getFlag() const {
    return Entity::Flags::PLAYER_CONTROLLER;
}

void Player::PlayerControllerComponent::jump(PhysicsComponent *physicsComponent) {
    btVector3 vel = physicsComponent->getRigidBody()->getLinearVelocity();
    m_isJumping = true;
    physicsComponent->getRigidBody()->applyCentralForce(btVector3(0, JUMP_SPEED, 0));
    physicsComponent->getRigidBody()->setLinearVelocity(btVector3(vel.x(), 1, vel.z()));
}

Player::~Player () {
    //dtor
}
