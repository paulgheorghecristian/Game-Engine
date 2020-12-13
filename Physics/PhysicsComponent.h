#ifndef PHYSICSCOMPONENT_H
#define PHYSICSCOMPONENT_H

#include "Component.h"
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
            SIMPLIFIED_MESH,
        };

        PhysicsComponent(BoundingBodyType type, glm::vec3 scale, float mass);
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

        virtual ~PhysicsComponent();

    protected:

    private:
        BoundingBodyType type;
        glm::vec3 boundingBodyScale;
        float bodyMass;

        btRigidBody *m_rigidBody;
};

#endif // PHYSICSCOMPONENT_H
