#ifndef PHYSICSMASTER_H
#define PHYSICSMASTER_H

#include "bullet/btBulletDynamicsCommon.h"
#include "Entity.h"
#include "PhysicsComponent.h"
#include <iostream>

class PhysicsMaster
{
    public:
        static void init(float gravity);
        static void destroy();
        static PhysicsMaster *getInstance();

        void update();
        btDynamicsWorld *getWorld();
        float getGravityAcceleration();

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
};

#endif // PHYSICSMASTER_H
