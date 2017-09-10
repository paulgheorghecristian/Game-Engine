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
        static void update();

        static PhysicsMaster *getInstance();
        static btDynamicsWorld *getWorld();

    protected:

    private:
        PhysicsMaster(float gravity);
        ~PhysicsMaster();

        static PhysicsMaster *m_instance;

        static btDynamicsWorld *world;
        static btDispatcher *dispatcher;
        static btBroadphaseInterface *broadsphase;
        static btCollisionConfiguration *collisionConfig;
        static btConstraintSolver *solver;

        static btRigidBody *planeRigidBody;
};

#endif // PHYSICSMASTER_H
