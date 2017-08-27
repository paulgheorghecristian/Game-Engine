#ifndef COMPONENTFACTORY_H
#define COMPONENTFACTORY_H

#include "rapidjson/document.h"
#include "RenderComponent.h"

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
