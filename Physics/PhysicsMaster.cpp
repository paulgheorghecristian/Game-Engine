#include "PhysicsMaster.h"

#include "Entity.h"
#include "Light.h"
#include "Common.h"

PhysicsMaster *PhysicsMaster::m_instance = NULL;

PhysicsMaster::PhysicsMaster(float gravity) : gravityAcc (gravity) {
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
