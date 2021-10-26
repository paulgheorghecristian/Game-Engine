#ifndef COMPONENTFACTORY_H
#define COMPONENTFACTORY_H

#include <functional>

#include "rapidjson/document.h"
#include "RenderComponent.h"
#include "TempAnimationComponent.h"
#include "PhysicsComponent.h"
#include "GrabComponent.h"
#include "BillboardComponent.h"
#include "ActionComponent.h"
#include "InstanceRenderComponent.h"
#include "SkeletalAnimationComponent.hpp"

class ComponentFactory
{
    public:
        static Component *createComponent(const rapidjson::Value::ConstMemberIterator &value);
        virtual ~ComponentFactory();

        static std::function<void(bool start, Entity *entity)> y_90_rotation;

    protected:

    private:
        ComponentFactory();
};

#endif // COMPONENTFACTORY_H
