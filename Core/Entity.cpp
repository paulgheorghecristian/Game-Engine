#include "Entity.h"

Entity::Entity(Transform &transform) : transform (transform) {

}

Entity *Entity::addChild(Entity *child) {
    children.push_back (child);

    return this;
}

Entity * Entity::addComponent(Component *component) {
    assert (component->getEntity() == NULL);
    assert (components[component->getFlag()] == NULL);

    component->setEntity (this);
    components[component->getFlag()] = component;

    return this;
}

void Entity::update() {
    for (auto const &it : components) {
        if (it.second != NULL) {
            it.second->update();
        }
    }

    for (unsigned int i = 0; i < children.size (); i++) {
        children[i]->update ();
    }
}

void Entity::input() {
    for (auto const &it : components) {
        if (it.second != NULL) {
            it.second->input();
        }
    }

    for (unsigned int i = 0; i < children.size (); i++) {
        children[i]->input ();
    }
}

void Entity::render() {
    for (auto const &it : components) {
        if (it.second != NULL) {
            it.second->render();
        }
    }

    for (unsigned int i = 0; i < children.size (); i++) {
        children[i]->render ();
    }
}

Transform &Entity::getTransform() {
    return transform;
}

Component *Entity::getComponent (const unsigned int flag) {
    return components[flag];
}

Entity::~Entity() {
    for (auto child : children) {
        delete child;
    }
    for (auto it : components) {
        delete it.second;
	}
}
