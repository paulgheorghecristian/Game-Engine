#include "AIPlayerFollowerComponent.hpp"

#include "PhysicsMaster.h"
#include "Common.h"

AIPlayerFollowerComponent::AIPlayerFollowerComponent(Player *player): player(player),
                                                                        lastFoundPathToPlayer(0),
                                                                        m_currentIdx(0),
                                                                        m_currTargetIdx(0),
                                                                        m_dirToCurrTarget(0),
                                                                        isMoving(false),
                                                                        isSameIdx(false) {
}

void AIPlayerFollowerComponent::input(Input &inputManager) {
}

void AIPlayerFollowerComponent::update() {
    // bezier B(t) = (1-t)^2*P0 + 2*(1-t)*t*P1 + t^2*P2
    // find player idx
    std::size_t playerIdx = 0;
    const glm::vec3 &playerPosition = player->getTransform().getPosition();
    glm::vec3 thisPosition = _entity->getTransform().getPosition();
    bool found = false, playerFound = false;

    m_currentIdx = findIdx(thisPosition, found);
    playerIdx = findIdx(playerPosition, playerFound);

    if (found == true && m_currTargetIdx == m_currentIdx) {
        isMoving = false;
        isSameIdx = true;
        lastFoundPathToPlayer.clear();

        m_dirToCurrTarget = playerPosition - thisPosition;
        m_dirToCurrTarget = glm::vec3(m_dirToCurrTarget.x, 0, m_dirToCurrTarget.z);
        float length = glm::length(m_dirToCurrTarget);
        m_dirToCurrTarget = glm::normalize(m_dirToCurrTarget);

        if (length < 30.0f || m_currentIdx != playerIdx) {
            isSameIdx = false;
        }
    }

    if (lastFoundPathToPlayer.size() == 0 && found == true) {
        if (playerFound == true) {
            PhysicsMaster::getInstance()->findShortestPath(m_currentIdx, playerIdx, lastFoundPathToPlayer);
            if (lastFoundPathToPlayer.size() == 1) {
                m_currTargetIdx = lastFoundPathToPlayer[0];
            } else {
                simplifyLastFoundPath();
            }
        }
    }

    if (lastFoundPathToPlayer.size() > 1 && isMoving == false) {
        std::size_t currIdx = lastFoundPathToPlayer[lastFoundPathToPlayer.size()-1];
        m_currTargetIdx = lastFoundPathToPlayer[lastFoundPathToPlayer.size()-2];

        m_dirToCurrTarget = getDirVecToIdx(m_currTargetIdx);
        isMoving = true;

        lastFoundPathToPlayer.erase(lastFoundPathToPlayer.begin() + (lastFoundPathToPlayer.size() - 1));
    }

    if (isMoving == true || isSameIdx == true) {
        glm::quat rotQuat = glm::angleAxis(glm::atan(m_dirToCurrTarget.x, m_dirToCurrTarget.z), glm::vec3(0,1,0));
        thisPosition = thisPosition + m_dirToCurrTarget * 0.5f;
        _entity->getTransform().setPosition(thisPosition);
        _entity->getTransform().setRotation(rotQuat);
    }
}

void AIPlayerFollowerComponent::init() {
    assert(_entity != NULL);

    bool found;
    const glm::vec3 &thisPosition = _entity->getTransform().getPosition();

    m_currentIdx = findIdx(thisPosition, found);
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

std::size_t AIPlayerFollowerComponent::findIdx(const glm::vec3 &thisPosition, bool &found) {
    float EPS = 0.3f;
    std::size_t foundIdx = 0;
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeEntities =
                            PhysicsMaster::getInstance()->getQuadTreeBodies();
    found = false;

    for (auto it: quadTreeEntities) {
        btGhostObject *body = it.second;

        btVector3 bScale = dynamic_cast<const btBoxShape* >(body->getCollisionShape())->getImplicitShapeDimensions();
        btVector3 bOrigin = body->getWorldTransform().getOrigin();

        glm::vec3 position = glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z());
        glm::vec3 scale = glm::vec3(bScale.x() * 2.0f, bScale.y() * 2.0f, bScale.z() * 2.0f);

        glm::vec2 xLimits = glm::vec2(position.x - scale.x * 0.5f, position.x + scale.x * 0.5f);
        glm::vec2 zLimits = glm::vec2(position.z - scale.z * 0.5f, position.z + scale.z * 0.5f);

        if (thisPosition.x >= xLimits.x - EPS &&
            thisPosition.x <= xLimits.y + EPS &&
            thisPosition.z >= zLimits.x - EPS &&
            thisPosition.z <= zLimits.y + EPS) {
            foundIdx = it.first;
            found = true;
            break;
        }
    }

    return foundIdx;
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

void AIPlayerFollowerComponent::simplifyLastFoundPath() {
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeEntities =
                            PhysicsMaster::getInstance()->getQuadTreeBodies();
    std::vector<std::size_t> toBeRemoved;
    int offset = 0;

    for (int idx = lastFoundPathToPlayer.size()-1; idx-2-offset >= 0; --idx) {
        std::size_t toCombine = lastFoundPathToPlayer[idx];
        std::size_t toCombine2 = lastFoundPathToPlayer[idx-2-offset];
        std::size_t maybeRemove = lastFoundPathToPlayer[idx-1-offset];

        bool found = true;
        btVector3 bOrigin = quadTreeEntities[toCombine]->getWorldTransform().getOrigin();
        btVector3 bScale = dynamic_cast<const btBoxShape* >(quadTreeEntities[toCombine]->getCollisionShape())->getImplicitShapeDimensions();
        btVector3 bOrigin2 = quadTreeEntities[toCombine2]->getWorldTransform().getOrigin();
        btVector3 bScale2 = dynamic_cast<const btBoxShape* >(quadTreeEntities[toCombine2]->getCollisionShape())->getImplicitShapeDimensions();

        glm::vec3 position = glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z());
        glm::vec3 position2 = glm::vec3(bOrigin2.x(), bOrigin2.y(), bOrigin2.z());

        glm::vec3 dir = glm::normalize(position2 - position);
        dir.y = 0;
        glm::vec3 perpDir = glm::normalize(glm::cross(dir, glm::vec3(0,1,0)));
        perpDir.y = 0;

        float steps[] = {-bScale.x()*1.2f, 0.0f, bScale.x()*1.2f};
        float steps2[] = {-bScale2.x()*1.2f, 0.0f, bScale2.x()*1.2f};

        for (std::size_t i = 0; i < sizeof(steps)/sizeof(steps[0]); i++) {
            position = glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z()) + perpDir * steps[i];
            position2 = glm::vec3(bOrigin2.x(), bOrigin2.y(), bOrigin2.z()) + perpDir * steps2[i];

            btVector3 from = btVector3(position.x, position.y, position.z);
            btVector3 to = btVector3(position2.x, position2.y, position2.z);

            btCollisionWorld::ClosestRayResultCallback rayCallback(from, to);
            PhysicsMaster::getInstance()->getWorld()->rayTest(from, to, rayCallback);

            bool localFound = rayCallback.hasHit();

            if (localFound == true) {
                UserData *data = (UserData *) rayCallback.m_collisionObject->getUserPointer();
                if (data == NULL ||
                    (data->type != PointerType::PHYSICS_BODY) ||
                    (data->pointer.entity == player)) {
                    continue;
                }
            }

            found = (found && !localFound);
        }

        if (found == true) {
            // maybeRemove can be removed;
            toBeRemoved.push_back(idx-1-offset);
            ++idx;
            ++offset;
        } else {
            idx = idx - offset;
            offset = 0;
        }
    }

    for (std::size_t p: toBeRemoved) {
        lastFoundPathToPlayer.erase(lastFoundPathToPlayer.begin() + p);
    }
}