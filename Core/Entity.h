#ifndef ENTITY_H
#define ENTITY_H

#include "Transform.h"
#include "Component.h"
#include <vector>
#include <unordered_map>

class Component;

class Entity
{
    public:
        Entity(Transform &transform);
        Entity *addChild(Entity *child);
        Entity *addComponent(Component *component);
        Transform &getTransform();
        Component *getComponent (const unsigned int flag);
        void input();
        void update();
        void render();
        virtual ~Entity();

        enum Flags {
            RENDERABLE = 1, /* contains RenderComponent */
            DYNAMIC, /* contains PhysicsComponent */
            EMIT_PARTICLES /* contains EmitParticlesComponent */
        };
    protected:

    private:
        Transform transform;
        std::vector<Entity *> children;
        std::unordered_map<unsigned int, Component *> components;
};

#endif // ENTITY_H
