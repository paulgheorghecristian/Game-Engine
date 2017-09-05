#ifndef PHYSICSMASTER_H
#define PHYSICSMASTER_H

#include "bullet/btBulletDynamicsCommon.h"
#include "Entity.h"
#include "PhysicsComponent.h"
#include <iostream>

class PhysicsMaster
{
    public:
        PhysicsMaster(std::vector<Entity *> &entities, float gravity);
        void update();
        virtual ~PhysicsMaster();

    protected:

    private:
        btDynamicsWorld *world;
        btDispatcher *dispatcher;
        btBroadphaseInterface *broadsphase;
        btCollisionConfiguration *collisionConfig;
        btConstraintSolver *solver;

        btRigidBody *planeRigidBody;

        std::vector<Entity *> &entities;
};

#endif // PHYSICSMASTER_H
