#ifndef COMPONENTFACTORY_H
#define COMPONENTFACTORY_H

#include "rapidjson/document.h"
#include "RenderComponent.h"
#include "TempAnimationComponent.h"
#include "PhysicsComponent.h"
#include "GrabComponent.h"

class ComponentFactory
{
    public:
        static Component *createComponent(const rapidjson::Value::ConstMemberIterator &value);
        virtual ~ComponentFactory();

    protected:

    private:
        ComponentFactory();
};

#endif // COMPONENTFACTORY_H
