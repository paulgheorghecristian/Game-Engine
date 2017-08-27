#include "Component.h"

Component::Component() {
    _entity = NULL;
}

void Component::setEntity (Entity * entity) {
    _entity = entity;
}

Entity * Component::getEntity () {
    return _entity;
}

Component::~Component()
{
}
