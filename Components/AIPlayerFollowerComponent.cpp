#include "AIPlayerFollowerComponent.hpp"

#include "PhysicsMaster.h"
#include "Common.h"
#include "DebugRenderer.hpp"

#include <cmath>

AIPlayerFollowerComponent::AIPlayerFollowerComponent(Player *player): player(player),
                                                                        lastFoundPathToPlayer(0),
                                                                        lastFoundPosToPlayer(0),
                                                                        m_currentIdx(0),
                                                                        m_currTargetIdx(0),
                                                                        m_dirToCurrTarget(0),
                                                                        m_Idx(0),
                                                                        m_interpolate(false),
                                                                        isSameIdx(false),
                                                                        stepInterp(0) {
    draw = false;
    stop = false;
}

void AIPlayerFollowerComponent::input(Input &inputManager) {
    if (inputManager.getKeyDown(SDLK_k)) {
        DebugRenderer::getInstance()->clearLinePoints();
        stop = !stop;
    }

    if (inputManager.getKeyDown(SDLK_l)) {
        draw = !draw;
    }
}

void AIPlayerFollowerComponent::update() {
    // bezier B(t) = (1-t)^2*P0 + 2*(1-t)*t*P1 + t^2*P2
    // find player idx
    std::size_t playerIdx = 0;
    const glm::vec3 &playerPosition = player->getTransform().getPosition();
    glm::vec3 thisPosition = _entity->getTransform().getPosition();
    bool found = false, playerFound = false;

    m_currentIdx = PhysicsMaster::getInstance()->findIdx(thisPosition, found);
    playerIdx = PhysicsMaster::getInstance()->findIdx(playerPosition, playerFound);

    if (found == false || playerFound == false)
        return;

    if (m_currentIdx == playerIdx) {
        lastFoundPosToPlayer.clear();
        lastFoundPathToPlayer.clear();

        isSameIdx = true;

        m_dirToCurrTarget = playerPosition - thisPosition;
        m_dirToCurrTarget.y = 0;
    } else {
        if (lastFoundPosToPlayer.size() == 0) {
            lastFoundPathToPlayer.clear();
            // from m_currentIdx to playerIdx
            PhysicsMaster::getInstance()->findShortestPath(playerIdx, m_currentIdx, lastFoundPathToPlayer);
            PhysicsMaster::getInstance()->simplifyLastFoundPath(lastFoundPathToPlayer);

            // exclude currentIdx
            if (lastFoundPathToPlayer.size() > 0)
                lastFoundPathToPlayer.erase(lastFoundPathToPlayer.begin());
            for (std::size_t i = 0; i < lastFoundPathToPlayer.size(); i++) {
                glm::vec3 pos = getPositionFromIdx(lastFoundPathToPlayer[i]);
                lastFoundPosToPlayer.push_back(pos);
                // if (draw == true)
                // DebugRenderer::getInstance()->addLinePoint(pos);
            }

            m_dirToCurrTarget = lastFoundPosToPlayer[0] - thisPosition;
            m_dirToCurrTarget.y = 0;
            m_interpolate = true;
            stepInterp = 0.0f;
        } else {
            if (glm::distance(glm::vec2(lastFoundPosToPlayer[0].x,lastFoundPosToPlayer[0].z),
                                glm::vec2(thisPosition.x,thisPosition.z)) < getScaleFromIdx(m_currentIdx).x*0.8) {
                lastFoundPosToPlayer.erase(lastFoundPosToPlayer.begin());

                m_dirToCurrTarget = lastFoundPosToPlayer[0] - thisPosition;
                m_dirToCurrTarget.y = 0;
                m_interpolate = true;
                stepInterp = 0.0f;
            }
        }
    }

    glm::vec3 m_dirToCurrTargetNorm = glm::normalize(m_dirToCurrTarget);
    rotQuat = glm::angleAxis(glm::atan(m_dirToCurrTargetNorm.x, m_dirToCurrTargetNorm.z), glm::vec3(0,1,0));

    float yRot = glm::eulerAngles(lastQuat).y;
    float yRot2 = glm::eulerAngles(rotQuat).y;

    if (m_interpolate == false || glm::abs(yRot-yRot2) < 0.3) {
        thisPosition = thisPosition + m_dirToCurrTargetNorm * 0.75f;
        _entity->getTransform().setPosition(thisPosition);
        _entity->getTransform().setRotation(rotQuat);
        lastQuat = rotQuat;
        m_interpolate = false;
    } else {
        glm::quat slerpQuat = glm::slerp(lastQuat, rotQuat, stepInterp);
        stepInterp += 0.3;
        if (stepInterp > 1.0f)
            m_interpolate = false;
        _entity->getTransform().setRotation(slerpQuat);
    }

    // TODO these component couplings can cause problems
    PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);

    if (physicsComponent != NULL) {
        btRigidBody *m_body = physicsComponent->getRigidBody();

        btTransform transform = m_body->getCenterOfMassTransform();

        transform.setOrigin(btVector3(thisPosition.x, thisPosition.y, thisPosition.z));
        m_body->setCenterOfMassTransform(transform);
    }
}

void AIPlayerFollowerComponent::init() {
    assert(_entity != NULL);

    bool found;
    const glm::vec3 &thisPosition = _entity->getTransform().getPosition();

    m_currentIdx = PhysicsMaster::getInstance()->findIdx(thisPosition, found);
    m_Idx = 0;
}

glm::vec3 AIPlayerFollowerComponent::getDirVecToIdx(std::size_t idx) {
    const glm::vec3 &thisPosition = _entity->getTransform().getPosition();
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeEntities =
                            PhysicsMaster::getInstance()->getQuadTreeBodies();

    btGhostObject *body = quadTreeEntities[idx];
    btVector3 bOrigin = body->getWorldTransform().getOrigin();

    glm::vec3 position = glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z());

    glm::vec3 res = glm::normalize(position - thisPosition);
    return glm::vec3(res.x, 0, res.z);
}

glm::vec3 AIPlayerFollowerComponent::getPositionFromIdx(std::size_t idx) {
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeEntities =
                            PhysicsMaster::getInstance()->getQuadTreeBodies();

    btGhostObject *body = quadTreeEntities[idx];
    btVector3 bOrigin = body->getWorldTransform().getOrigin();

    glm::vec3 position = glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z());
    return position;
}

glm::vec3 AIPlayerFollowerComponent::getScaleFromIdx(std::size_t idx) {
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeEntities =
                            PhysicsMaster::getInstance()->getQuadTreeBodies();

    btGhostObject *body = quadTreeEntities[idx];
    btVector3 bScale = dynamic_cast<const btBoxShape* >(body->getCollisionShape())->getImplicitShapeDimensions();

    glm::vec3 scale = glm::vec3(bScale.x(), bScale.y(), bScale.z());
    return scale;
}

const unsigned int AIPlayerFollowerComponent::getFlag() const {
    return Entity::Flags::AIPLAYERFOLLOWER;
}

std::string AIPlayerFollowerComponent::jsonify() {
    std::string res("");

    // res += "\"ActionComponent\":{";
    // res += "\"radius\":" + std::to_string(m_radius) + ",";
    // res += "\"action\":\"" + m_actionName + "\"}";

    return res;
}
