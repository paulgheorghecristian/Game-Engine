#include "PhysicsMaster.h"

PhysicsMaster::PhysicsMaster(std::vector<Entity *> &entities, float gravity) : entities (entities) {
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
    info.m_restitution = 0.3;
    info.m_friction = 1.0;
    planeRigidBody = new btRigidBody(info);
    world->addRigidBody(planeRigidBody);

    for (auto const &entity : entities) {
        Component *component;
        if ((component = entity->getComponent(Entity::Flags::DYNAMIC)) != NULL) {
            PhysicsComponent *physicsComponent = (PhysicsComponent *) component;
            physicsComponent->init();
            world->addRigidBody (physicsComponent->getRigidBody());
        }
    }
}

void PhysicsMaster::update() {
    world->stepSimulation(btScalar(0.5f));
}

PhysicsMaster::~PhysicsMaster() {
    delete planeRigidBody->getMotionState();
    delete planeRigidBody;

    delete dispatcher;
    delete broadsphase;
    delete collisionConfig;
    delete solver;
    delete world;
}
