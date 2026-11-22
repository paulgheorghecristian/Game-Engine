#ifndef PHYSICSCOMPONENT_H
#define PHYSICSCOMPONENT_H

#include "Component.h"
#include "Mesh.h"
#include "glm/glm.hpp"
#include "bullet/btBulletDynamicsCommon.h"
#include "glm/gtc/type_ptr.hpp"
#include "PhysicsMaster.h"

class PhysicsMaster;

class PhysicsComponent : public Component
{
    public:
        enum BoundingBodyType {
            CUBE,
            SPHERE,
            CAPSULE,
            MESH,
        };

        PhysicsComponent(BoundingBodyType type, glm::vec3 scale,
                        float mass, bool shouldUpdate = true, Mesh *mesh = nullptr);
        void input(Input &inputManager);
        void update();
        void render();
        void init();
        void enable();
        void disable();
        const unsigned int getFlag() const;
        btRigidBody *getRigidBody();
        const glm::vec3 &getBoundingBodyScale();

        std::string jsonify() override;
        /* mesh bodies: the mesh file they were built from (for saving) */
        void setMeshPath(const std::string &path) { m_meshPath = path; }

        virtual ~PhysicsComponent();

    protected:

    private:
        BoundingBodyType type;
        std::string m_meshPath;
        glm::vec3 boundingBodyScale;
        float bodyMass;

        bool m_shouldUpdate;

        btRigidBody *m_rigidBody;
        std::vector<int> indices;
        std::vector<btScalar> vertices;
        btTriangleIndexVertexArray *m_boundingMeshData;
};

#endif // PHYSICSCOMPONENT_H
