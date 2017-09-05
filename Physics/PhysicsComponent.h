#ifndef PHYSICSCOMPONENT_H
#define PHYSICSCOMPONENT_H

#include "Component.h"
#include "glm/glm.hpp"
#include "bullet/btBulletDynamicsCommon.h"
#include "glm/gtc/type_ptr.hpp"

class PhysicsComponent : public Component
{
    public:
        enum BoundingBodyType {
            CUBE,
            SPHERE,
            SIMPLIFIED_MESH,
        };

        PhysicsComponent(BoundingBodyType type, glm::vec3 scale, float mass);
        void input();
        void update();
        void render();
        void init();
        const unsigned int getFlag() const;
        btRigidBody *getRigidBody();
        virtual ~PhysicsComponent();

    protected:

    private:
        BoundingBodyType type;
        glm::vec3 boundingBodyScale;
        float bodyMass;

        btRigidBody *m_rigidBody;
};

#endif // PHYSICSCOMPONENT_H
