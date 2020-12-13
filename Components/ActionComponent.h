#ifndef ACTIONCOMPONENT_H
#define ACTIONCOMPONENT_H

#include <functional>

#include "Component.h"
#include "glm/glm.hpp"
#include "bullet/btBulletDynamicsCommon.h"
#include "glm/gtc/type_ptr.hpp"
#include "PhysicsMaster.h"

#include "btGhostObject.h"

class ActionComponent : public Component
{
    public:
        ActionComponent(float radius, std::function<void(bool start, Entity *entity)> action,
                        const std::string &actionName);
        void input(Input &inputManager);
        void update();
        void render() {};
        void init();
        const unsigned int getFlag() const;

        std::string jsonify() override;

        virtual ~ActionComponent();

    protected:

    private:
        btGhostObject *m_ghostObj;
        float m_radius;
        std::function<void(bool start, Entity *entity)> m_action;
        std::string m_actionName;
};

#endif // ACTIONCOMPONENT_H
