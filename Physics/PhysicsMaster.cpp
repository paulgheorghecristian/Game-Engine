#include "PhysicsMaster.h"

PhysicsMaster *PhysicsMaster::m_instance = NULL;

btDynamicsWorld *PhysicsMaster::world;
btDispatcher *PhysicsMaster::dispatcher;
btBroadphaseInterface *PhysicsMaster::broadsphase;
btCollisionConfiguration *PhysicsMaster::collisionConfig;
btConstraintSolver *PhysicsMaster::solver;
btRigidBody *PhysicsMaster::planeRigidBody;

PhysicsMaster::PhysicsMaster(float gravity) {
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
    world->stepSimulation(btScalar(0.5f));
}

btDynamicsWorld *PhysicsMaster::getWorld() {
    return world;
}

PhysicsMaster::~PhysicsMaster() {
    //delete planeRigidBody->getMotionState();
    //delete planeRigidBody;

    delete world;
}
