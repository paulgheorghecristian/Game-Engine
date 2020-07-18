#ifndef COMPONENT_H
#define COMPONENT_H

#include "Entity.h"
#include "Input.h"

class Entity;

class Component
{
    public:
        Component();
        virtual void input(Input &inputManager) = 0;
        virtual void update() = 0;
        virtual void render() = 0;
        virtual const unsigned int getFlag() const = 0;
        virtual void init();
        virtual void enable();
        virtual void disable();
        void setEntity (Entity *entity);
        Entity *getEntity ();
        virtual ~Component();

    protected:
        Entity *_entity;
        bool m_disabled;
    private:
};

#endif // COMPONENT_H
