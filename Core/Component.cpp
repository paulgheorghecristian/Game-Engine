#include "Component.h"

Component::Component() {
    _entity = NULL;
    m_disabled = false;
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

void Component::enable() {
    m_disabled = false;
}

void Component::disable() {
    m_disabled = true;
}

std::string Component::jsonify() {
    return std::string("");
}
