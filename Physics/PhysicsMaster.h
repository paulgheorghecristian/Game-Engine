#ifndef PHYSICSMASTER_H
#define PHYSICSMASTER_H

#include "bullet/btBulletDynamicsCommon.h"
#include "PhysicsComponent.h"
#include <iostream>

#include <set>

class Entity;
class Light;

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
        void performMouseRayIntersection(float mouseX, float mouseY,
                                        int width, int height,
                                        const glm::vec3 &cameraPosition, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix);

        const std::set<Entity *> &getForwardIntersectRes() { return forwardIntersectEntities; }
        const std::set<Entity *> &getMouseRayInteresectEntities() { return mouseRayIntersectEntities; }
        const std::set<Light *> &getMouseRayInteresectLights() { return mouseRayIntersectLights; }

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
        std::set<Entity *> mouseRayIntersectEntities;
        std::set<Light *> mouseRayIntersectLights;

        glm::vec3 currMouseRayWorldPos;

        glm::vec3 getWorldSpaceMouseRay(float mouseX, float mouseY,
                                        int width, int height,
                                        const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix);

};

#endif // PHYSICSMASTER_H
