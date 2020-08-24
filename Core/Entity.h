#ifndef ENTITY_H
#define ENTITY_H

#include "Transform.h"
#include "Component.h"
#include <vector>
#include <unordered_map>
#include "Input.h"
#include "MemoryPoolInterface.h"

class Component;

class Entity : public MemoryPoolInterface<Entity>
{
    public:
        Entity();
        Entity *addChild(Entity *child);
        Entity *addComponent(Component *component);
        Entity *updateComponent (Component *component);
        void removeComponent (unsigned int flag);
        Transform &getTransform();
        void setTransform(const Transform &transform);
        Component *getComponent (const unsigned int flag);
        void input(Input &inputManager);
        void update();
        void render();
        virtual ~Entity();

        enum Flags {
            RENDERABLE = 1, /* contains RenderComponent */
            DYNAMIC, /* contains PhysicsComponent */
            EMIT_PARTICLES, /* contains EmitParticlesComponent */
            ANIMATED,
            THIRD_PERSON_CAMERA, /* only for the player, maybe I should hide this? */
            PLAYER_CONTROLLER,
            GRAB,
            ACTION,
        };
    protected:
        Transform transform;
        std::vector<Entity *> children;
        std::unordered_map<unsigned int, Component *> components;
    private:
};

#endif // ENTITY_H
