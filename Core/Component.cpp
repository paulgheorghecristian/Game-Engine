#include "Component.h"

Component::Component() {
    _entity = NULL;
}

void Component::setEntity (Entity *entity) {
    _entity = entity;
}

Entity *Component::getEntity () {
    return _entity;
}

void Component::init(){}

Component::~Component()
{
}
