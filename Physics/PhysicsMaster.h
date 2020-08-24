#ifndef PHYSICSMASTER_H
#define PHYSICSMASTER_H

#include "bullet/btBulletDynamicsCommon.h"
#include "PhysicsComponent.h"
#include <iostream>

#include <set>

class Entity;

class PhysicsMaster
{
    public:
        static void init(float gravity);
        static void destroy();
        static PhysicsMaster *getInstance();

        void update();
        btDynamicsWorld *getWorld();
        float getGravityAcceleration();

        void performRayTestWithCamForward(const glm::vec3 &position, const glm::vec3 &forward);
        const std::set<Entity *> &getForwardIntersectRes() {return forwardIntersectEntities;}

    protected:

    private:
        PhysicsMaster(float gravity);
        ~PhysicsMaster();

        static PhysicsMaster *m_instance;

        btDynamicsWorld *world;
        btDispatcher *dispatcher;
        btBroadphaseInterface *broadsphase;
        btCollisionConfiguration *collisionConfig;
        btConstraintSolver *solver;

        btRigidBody *planeRigidBody;
        float gravityAcc;

        std::set<Entity *> forwardIntersectEntities;
};

#endif // PHYSICSMASTER_H
