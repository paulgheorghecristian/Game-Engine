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

        inline void setToBeRemoved(bool remove) { m_to_be_removed = remove; }
        inline bool isToBeRemoved() { return m_to_be_removed; }

        virtual std::string jsonify();

        /* the map JSON this entity was loaded from (saving writes it back with
         * the current transform, so nothing the engine can't serialize is lost) */
        inline void setSourceJson(const std::string &json) { m_sourceJson = json; }
        inline const std::string &getSourceJson() const { return m_sourceJson; }

        enum Flags {
            RENDERABLE = 1, /* contains RenderComponent */
            DYNAMIC, /* contains PhysicsComponent */
            EMIT_PARTICLES, /* contains EmitParticlesComponent */
            ANIMATED,
            THIRD_PERSON_CAMERA, /* only for the player, maybe I should hide this? */
            PLAYER_CONTROLLER,
            GRAB,
            BILL,
            ACTION,
            INSTANCE_RENDERABLE,
            AIPLAYERFOLLOWER,
            PARTICLE_EMITTER,
            TRANSFORM_ANIMATION /* TempAnimationComponent (not ANIMATED: that one
                                 * is the skeletal animation the renderer draws) */
        };
    protected:
        Transform m_transform;
        std::vector<Entity *> children;
        std::unordered_map<unsigned int, Component *> components;
    private:

        bool m_to_be_removed;
        std::string m_sourceJson;
};

#endif // ENTITY_H
