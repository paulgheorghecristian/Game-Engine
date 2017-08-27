#ifndef COMPONENT_H
#define COMPONENT_H

#include "Entity.h"

class Entity;

class Component
{
    public:
        Component();
        virtual void input() = 0;
        virtual void update() = 0;
        virtual void render() = 0;
        virtual const unsigned int getFlag() const = 0;
        void setEntity (Entity * entity);
        Entity * getEntity ();
        virtual ~Component();

    protected:
        Entity * _entity;
    private:
};

#endif // COMPONENT_H
