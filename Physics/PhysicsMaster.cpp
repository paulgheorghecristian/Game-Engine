#include "PhysicsMaster.h"

#include "Entity.h"
#include "Light.h"
#include "Common.h"

#include <algorithm>
#include <queue>
#include <functional>
#include <unordered_set>

PhysicsMaster *PhysicsMaster::m_instance = NULL;

PhysicsMaster::PhysicsMaster(float gravity) : gravityAcc (gravity), quadTree(glm::vec2(0), 1024, 6) {
    btTransform t;

    collisionConfig = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfig);
    broadsphase = new btDbvtBroadphase();
    solver = new btSequentialImpulseConstraintSolver();

    world = new btDiscreteDynamicsWorld(dispatcher,
                                        broadsphase,
                                        solver,
                                        collisionConfig);

    world->setGravity(btVector3(0, gravity, 0));

    t.setIdentity();
    t.setOrigin(btVector3(0, 0, 0));

    btStaticPlaneShape *plane = new btStaticPlaneShape(btVector3(0, 1, 0), btScalar(0));
    btMotionState *motion = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo info(0.0, motion, plane);
    info.m_restitution = 0.7;
    info.m_friction = 0.5;
    planeRigidBody = new btRigidBody(info);
    world->addRigidBody(planeRigidBody);
}

btGhostObject *PhysicsMaster::createBoxRigidBody(glm::vec3 position, glm::vec3 scale) {
    btTransform t;
    btCollisionShape *collisionShape = NULL;

    t.setIdentity();
    t.setOrigin (btVector3(position.x, position.y, position.z));

    collisionShape = new btBoxShape(btVector3(scale.x / 2.0f,
                                                scale.y / 2.0f,
                                                scale.z / 2.0f));
    collisionShape->setMargin(0.0f);

    btGhostObject *rigidBody = new btGhostObject();
    rigidBody->setCollisionShape(collisionShape);
    rigidBody->setWorldTransform(t);
    return rigidBody;
}


void PhysicsMaster::startGraphCreation() {
    gContactAddedCallback = PhysicsMaster::bulletCollisionCallback;

    quadTreeBodies.clear();

    // create quad tree root node
    btGhostObject *rootRigidBody = createBoxRigidBody(glm::vec3(-200.0f, 20.0f, 0.0f), glm::vec3(2048.0f, 5.0f, 2048.0f));
    rootRigidBody->setCollisionFlags(rootRigidBody->getCollisionFlags() |
                              btCollisionObject::CF_NO_CONTACT_RESPONSE |
                              btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    UserData *userData = new UserData();
    userData->type = PointerType::QUADT_NODE;
    userData->pointer.node = &quadTree.getRoot();
    rootRigidBody->setUserPointer((void *) userData);
    world->addCollisionObject(rootRigidBody);
    // end quad tree root node creation

    quadTreeBodies[userData->pointer.node->m_idx] = rootRigidBody;

    for (int i = 0; i < quadTree.getMaxLevel(); i++) {
        world->performDiscreteCollisionDetection();
    }

    quadTree.createGraph(&quadTree.getRoot());
}

void PhysicsMaster::endGraphCreation() {
    gContactAddedCallback = NULL;

    for (auto iter = quadTreeBodies.begin(); iter != quadTreeBodies.end(); ++iter) {
        world->removeCollisionObject(iter->second);
    }
}

PhysicsMaster *PhysicsMaster::getInstance() {
    return m_instance;
}

void PhysicsMaster::init(float gravity) {
    if (m_instance == NULL) {
        m_instance = new PhysicsMaster (gravity);
    }
}

void PhysicsMaster::destroy() {
    if (m_instance != NULL) {
        delete m_instance;
    }
}

void PhysicsMaster::update() {
    world->stepSimulation(btScalar(0.15f), 50, btScalar(1.)/btScalar(60.));
}

btDynamicsWorld *PhysicsMaster::getWorld() {
    return world;
}

float PhysicsMaster::getGravityAcceleration() {
    return gravityAcc;
}

void PhysicsMaster::performRayTestWithCamForward(const glm::vec3 &position, const glm::vec3 &forward) {
    forwardIntersectEntities.clear();
    {
        // perform ray intersection with object
        const glm::vec3 &currWorldPos = position;
        const glm::vec3 &dir = forward;

        glm::vec3 from_glm = currWorldPos+dir*2.0f;
        glm::vec3 to_glm = currWorldPos+dir*25.0f;

        btVector3 from = btVector3(from_glm.x, from_glm.y, from_glm.z);
        btVector3 to = btVector3(to_glm.x, to_glm.y, to_glm.z);

        btCollisionWorld::AllHitsRayResultCallback rayCallback(from, to);
        world->rayTest(from, to, rayCallback);

        for (unsigned int i = 0; i < rayCallback.m_hitFractions.size(); i++) {
            UserData *data = (UserData *) rayCallback.m_collisionObjects[i]->getUserPointer();

            if (data && data->type == PointerType::ENTITY) {
                forwardIntersectEntities.insert(data->pointer.entity);
            }
        }
    }
}

void PhysicsMaster::performMouseRayIntersection(float mouseX, float mouseY, int width,
                                                int height, const glm::vec3 &cameraPosition,
                                                const glm::mat4 &projectionMatrix,
                                                const glm::mat4 &viewMatrix) {
    mouseRayIntersectEntities.clear();
    mouseRayIntersectLights.clear();
    {
        // perform ray intersection with object
        const glm::vec3 &worldSpaceMouseRay = getWorldSpaceMouseRay(mouseX, mouseY,
                                                                    width, height,
                                                                    projectionMatrix, viewMatrix);

        glm::vec3 from_glm = cameraPosition+worldSpaceMouseRay*10.0f;
        glm::vec3 to_glm = cameraPosition+worldSpaceMouseRay*1000.0f;

        btVector3 from = btVector3(from_glm.x, from_glm.y, from_glm.z);
        btVector3 to = btVector3(to_glm.x, to_glm.y, to_glm.z);

        btCollisionWorld::AllHitsRayResultCallback rayCallback(from, to);
        world->rayTest(from, to, rayCallback);

        for (unsigned int i = 0; i < rayCallback.m_hitFractions.size(); i++) {
            UserData *data = (UserData *) rayCallback.m_collisionObjects[i]->getUserPointer();
            if (data) {
                if (data->type == PointerType::ENTITY) {
                    mouseRayIntersectEntities.insert(data->pointer.entity);
                } else if (data->type == PointerType::LIGHT) {
                    mouseRayIntersectLights.insert(data->pointer.light);
                }
            }
        }
    }
}

glm::vec3 PhysicsMaster::getWorldSpaceMouseRay(float mouseX, float mouseY,
                                                int width, int height,
                                                const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) {
    float ndcX = 2.0f*mouseX / width - 1;
    float ndcY = 2.0f*(height-mouseY) / height - 1;

    glm::vec4 ndc(ndcX, ndcY, -1.0f, 1.0f);

    glm::vec4 clipSpace = glm::inverse(projectionMatrix) * ndc;
    glm::vec4 eyeSpace = glm::vec4(clipSpace.x, clipSpace.y, -1.0f, 0.0f);

    glm::vec4 worldSpace = glm::inverse(viewMatrix) * eyeSpace;

    return glm::normalize(glm::vec3(worldSpace.x, worldSpace.y, worldSpace.z));
}

bool PhysicsMaster::bulletCollisionCallback(btManifoldPoint& cp, const btCollisionObjectWrapper *obj1, int id1, int index1,
                                            const btCollisionObjectWrapper *obj2, int id2, int index2)
{
    const float OFFSET = 1.0f;
    const btCollisionObjectWrapper *quadTreeNode = NULL;
    if (obj1->getCollisionObject()->getUserPointer() != NULL &&
        (obj1->getCollisionObject()->getCollisionFlags() &
        btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK)) {
        quadTreeNode = obj1;
	} else if (obj2->getCollisionObject()->getUserPointer() != NULL &&
                (obj2->getCollisionObject()->getCollisionFlags() &
                btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK)) {
        quadTreeNode = obj2;
	}

    if (quadTreeNode == NULL)
        return false;

    UserData *uData;
    uData = (UserData *) quadTreeNode->getCollisionObject()->getUserPointer();

    if (uData == NULL || uData->type != PointerType::QUADT_NODE)
        return false;

    QuadTree::Node *node = (QuadTree::Node *) uData->pointer.node;
    node->m_data.isBlocked = true;
    // TODO const_cast is kinda hacky
    btGhostObject *thisRigidBody = const_cast<btGhostObject *>(btGhostObject::upcast(quadTreeNode->getCollisionObject()));

    bool rc = PhysicsMaster::getInstance()->getQuadTree().divide(node);

    if (rc == false)
        return false;

    PhysicsMaster::getInstance()->getWorld()->removeCollisionObject(thisRigidBody);
    std::unordered_map<std::size_t, btGhostObject *> &quadTreeBodies = PhysicsMaster::getInstance()->getQuadTreeBodies();
    quadTreeBodies.erase(node->m_idx);

    btVector3 bScale = dynamic_cast<const btBoxShape* >(quadTreeNode->getCollisionObject()->getCollisionShape())->getImplicitShapeDimensions();
    btVector3 bOrigin = thisRigidBody->getWorldTransform().getOrigin();

    glm::vec3 scale = glm::vec3(bScale.x() * 2.0f + OFFSET,
                                bScale.y() * 2.0f,
                                bScale.z() * 2.0f + OFFSET);

    btGhostObject *firstNode = PhysicsMaster::getInstance()->createBoxRigidBody(glm::vec3(bOrigin.x() - scale.x / 4, bOrigin.y(), bOrigin.z() + scale.z / 4),
                                                                            glm::vec3(scale.x / 2 - OFFSET, scale.y, scale.z / 2 - OFFSET));
    btGhostObject *secondNode = PhysicsMaster::getInstance()->createBoxRigidBody(glm::vec3(bOrigin.x() + scale.x / 4, bOrigin.y(), bOrigin.z() + scale.z / 4),
                                                                            glm::vec3(scale.x / 2 - OFFSET, scale.y, scale.z / 2 - OFFSET));
    btGhostObject *thirdNode = PhysicsMaster::getInstance()->createBoxRigidBody(glm::vec3(bOrigin.x() + scale.x / 4, bOrigin.y(), bOrigin.z() - scale.z / 4),
                                                                            glm::vec3(scale.x / 2 - OFFSET, scale.y, scale.z / 2 - OFFSET));
    btGhostObject *fourthNode = PhysicsMaster::getInstance()->createBoxRigidBody(glm::vec3(bOrigin.x() - scale.x / 4, bOrigin.y(), bOrigin.z() - scale.z / 4),
                                                                            glm::vec3(scale.x / 2 - OFFSET, scale.y, scale.z / 2 - OFFSET));

    firstNode->setCollisionFlags(firstNode->getCollisionFlags() |
                              btCollisionObject::CF_NO_CONTACT_RESPONSE |
                              btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    uData = new UserData();
    uData->type = PointerType::QUADT_NODE;
    uData->pointer.node = node->m_children[0];
    firstNode->setUserPointer((void *) uData);

    secondNode->setCollisionFlags(secondNode->getCollisionFlags() |
                              btCollisionObject::CF_NO_CONTACT_RESPONSE |
                              btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    uData = new UserData();
    uData->type = PointerType::QUADT_NODE;
    uData->pointer.node = node->m_children[1];
    secondNode->setUserPointer((void *) uData);

    thirdNode->setCollisionFlags(thirdNode->getCollisionFlags() |
                                btCollisionObject::CF_NO_CONTACT_RESPONSE |
                                btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    uData = new UserData();
    uData->type = PointerType::QUADT_NODE;
    uData->pointer.node = node->m_children[2];
    thirdNode->setUserPointer((void *) uData);

    fourthNode->setCollisionFlags(fourthNode->getCollisionFlags() |
                                btCollisionObject::CF_NO_CONTACT_RESPONSE |
                                btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
    uData = new UserData();
    uData->type = PointerType::QUADT_NODE;
    uData->pointer.node = node->m_children[3];
    fourthNode->setUserPointer((void *) uData);

    PhysicsMaster::getInstance()->getWorld()->addCollisionObject(firstNode);
    PhysicsMaster::getInstance()->getWorld()->addCollisionObject(secondNode);
    PhysicsMaster::getInstance()->getWorld()->addCollisionObject(thirdNode);
    PhysicsMaster::getInstance()->getWorld()->addCollisionObject(fourthNode);

    quadTreeBodies[node->m_children[0]->m_idx] = firstNode;
    quadTreeBodies[node->m_children[1]->m_idx] = secondNode;
    quadTreeBodies[node->m_children[2]->m_idx] = thirdNode;
    quadTreeBodies[node->m_children[3]->m_idx] = fourthNode;

    return false;
}

void PhysicsMaster::findShortestPath(std::size_t source, std::size_t target, std::vector<std::size_t> &path) {
    std::unordered_map<std::size_t, float> dist;
    std::unordered_map<std::size_t, int> prev;
    auto cmp = [&, this](std::size_t node1, std::size_t node2) {
        btGhostObject *node1Obj = this->quadTreeBodies[node1];
        btGhostObject *node2Obj = this->quadTreeBodies[node2];
        btGhostObject *sourceObj = this->quadTreeBodies[source];

        assert(node1Obj != NULL);
        assert(node2Obj != NULL);
        assert(sourceObj != NULL);

        btVector3 bOrigin1 = node1Obj->getWorldTransform().getOrigin();
        btVector3 bOrigin2 = node2Obj->getWorldTransform().getOrigin();
        btVector3 bSource = sourceObj->getWorldTransform().getOrigin();

        glm::vec3 or1 = glm::vec3(bOrigin1.x(), bOrigin1.y(), bOrigin1.z());
        glm::vec3 or2 = glm::vec3(bOrigin2.x(), bOrigin2.y(), bOrigin2.z());
        glm::vec3 sourceVec = glm::vec3(bSource.x(), bSource.y(), bSource.z());

        return glm::distance(or1, sourceVec) > glm::distance(or2, sourceVec);
    };
    std::priority_queue<std::size_t, std::vector<std::size_t>, decltype(cmp)> Q(cmp);
    std::unordered_set<std::size_t> priorityQueueElements;

    std::unordered_map<std::size_t, std::unordered_set<std::size_t>> &nodes = quadTree.getGraph().getNodes();

    dist[source] = 0;
    prev[source] = -1;
    Q.push(source);
    priorityQueueElements.insert(source);

    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        std::size_t v = iter->first;

        if (v != source) {
            dist[v] = std::numeric_limits<float>::max();
            prev[v] = -1;
        }
    }

    while (Q.size() > 0) {
        std::size_t u = Q.top();
        Q.pop();

        priorityQueueElements.erase(priorityQueueElements.find(u), priorityQueueElements.end()); 

        for (auto iter = nodes[u].begin(); iter != nodes[u].end(); ++iter) {
            std::size_t v = *iter;
            btGhostObject *node1Obj = quadTreeBodies[u];
            btGhostObject *node2Obj = quadTreeBodies[v];

            assert(node1Obj != NULL);
            assert(node2Obj != NULL);

            btVector3 bOrigin1 = node1Obj->getWorldTransform().getOrigin();
            btVector3 bOrigin2 = node2Obj->getWorldTransform().getOrigin();
            glm::vec3 or1 = glm::vec3(bOrigin1.x(), bOrigin1.y(), bOrigin1.z());
            glm::vec3 or2 = glm::vec3(bOrigin2.x(), bOrigin2.y(), bOrigin2.z());

            float alt = dist[u] + glm::distance(or1, or2);

            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                if (priorityQueueElements.find(v) == priorityQueueElements.end()) {
                    Q.push(v);
                    priorityQueueElements.insert(v);
                }
            }
        }

        if (u == target)
            break;
    }

    int p = target;
    if (prev[p] != -1 || target == source) {
        while (p != -1) {
            path.push_back(p);
            p = prev[p];
        }
    }
}

PhysicsMaster::~PhysicsMaster() {
    /* TODO i think these are destroyed by world destr */
    //delete planeRigidBody->getMotionState();
    //delete planeRigidBody;

    /*delete collisionConfig;
    delete dispatcher;
    delete broadsphase;
    delete solver;*/

    delete world;
}
