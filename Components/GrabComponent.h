#ifndef GRABCOMPONENT_H
#define GRABCOMPONENT_H

#include "Component.h"
#include "glm/glm.hpp"
#include "bullet/btBulletDynamicsCommon.h"
#include "glm/gtc/type_ptr.hpp"
#include "PhysicsMaster.h"

#include "btGhostObject.h"

class GrabComponent : public Component
{
    public:
        GrabComponent(float radius);
        void input(Input &inputManager);
        void update();
        void render();
        void init();
        const unsigned int getFlag() const;
        virtual ~GrabComponent();

    protected:

    private:
        btGhostObject *m_ghostObj;
        glm::vec3 worldPos;
        glm::vec3 worldRot;
        float m_radius;
        bool performRayTest, showGUI;
};

#endif // GRABCOMPONENT_H
