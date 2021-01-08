#ifndef PHYSICSMASTER_H
#define PHYSICSMASTER_H

#include "bullet/btBulletDynamicsCommon.h"
#include "PhysicsComponent.h"
#include "QuadTree.hpp"
#include <iostream>

#include <set>

#include "btGhostObject.h"

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
        btGhostObject *createBoxRigidBody(glm::vec3 position, glm::vec3 scale);
        std::unordered_map<std::size_t, btGhostObject *> &getQuadTreeBodies() { return quadTreeBodies; }

        void findShortestPath(std::size_t source, std::size_t target, std::vector<std::size_t> &path);

        inline QuadTree &getQuadTree() { return quadTree; }

        void performRayTestWithCamForward(const glm::vec3 &position, const glm::vec3 &forward);
        void performMouseRayIntersection(float mouseX, float mouseY,
                                        int width, int height,
                                        const glm::vec3 &cameraPosition, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix);

        void startGraphCreation();
        void endGraphCreation();

        const std::set<Entity *> &getForwardIntersectRes() { return forwardIntersectEntities; }
        const std::set<Entity *> &getMouseRayInteresectEntities() { return mouseRayIntersectEntities; }
        const std::set<Light *> &getMouseRayInteresectLights() { return mouseRayIntersectLights; }

        static bool bulletCollisionCallback(btManifoldPoint& cp, const btCollisionObjectWrapper *obj1, int id1, int index1,
                                            const btCollisionObjectWrapper *obj2, int id2, int index2);

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

        // change to unordered_set
        std::set<Entity *> forwardIntersectEntities;
        std::set<Entity *> mouseRayIntersectEntities;
        std::set<Light *> mouseRayIntersectLights;

        glm::vec3 currMouseRayWorldPos;

        glm::vec3 getWorldSpaceMouseRay(float mouseX, float mouseY,
                                        int width, int height,
                                        const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix);
        QuadTree quadTree;
        std::unordered_map<std::size_t, btGhostObject *> quadTreeBodies;
};

#endif // PHYSICSMASTER_H
