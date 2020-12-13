#include "Entity.h"

Entity::Entity() {

}

Entity *Entity::addChild(Entity *child) {
    children.push_back (child);

    return this;
}

Entity *Entity::addComponent(Component *component) {
    if (component == NULL) {
        return this;
    }
    assert (component->getEntity() == NULL);
    assert (components[component->getFlag()] == NULL);

    component->setEntity (this);
    components[component->getFlag()] = component;

    component->init();
    return this;
}

Entity *Entity::updateComponent (Component *component) {
    if (components[component->getFlag()] != NULL) {
        Component *prevComponent = components[component->getFlag()];
        components[component->getFlag()] = NULL;
        delete prevComponent;
    }

    return this->addComponent (component);
}

void Entity::removeComponent (unsigned int flag) {
    Component *prevComponent = components[flag];

    components[flag] = NULL;
    delete prevComponent;
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

void Entity::input(Input &inputManager) {
    for (auto const &it : components) {
        if (it.second != NULL) {
            it.second->input(inputManager);
        }
    }

    for (unsigned int i = 0; i < children.size (); i++) {
        children[i]->input (inputManager);
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
    return m_transform;
}

void Entity::setTransform(const Transform &transform) {
    this->m_transform = transform;
}

Component *Entity::getComponent (const unsigned int flag) {
    return components[flag];
}

std::string Entity::jsonify() {
    glm::vec3 pos, scale;
    glm::quat rot;
    std::string res("");
    bool hasComp = false;

    pos = m_transform.getPosition();
    rot = m_transform.getRotation();
    scale = m_transform.getScale();

    res += "\"Transform\":{";

    res += "\"position\":[" + std::to_string(pos.x) + ","
        + std::to_string(pos.y) + "," + std::to_string(pos.z) + "],";
    res += "\"rotation\":[" + std::to_string(rot.x) + ","
        + std::to_string(rot.y) + "," + std::to_string(rot.z) + "," + std::to_string(rot.w) + "],";
    res += "\"scale\":[" + std::to_string(scale.x) + ","
        + std::to_string(scale.y) + "," + std::to_string(scale.z) + "]";

    res += "},";

    res += "\"Components\":{";
    for (auto& it: components) {
        if (it.second != NULL) {
            std::string jsonStr = it.second->jsonify();

            if (jsonStr.size() == 0) continue;

            res += jsonStr;
            res += ",";
            hasComp = true;
        }
    }
    if (hasComp > 0) {
        res.pop_back();
    }
    res += "}";

    return res;
}

Entity::~Entity() {
    for (auto child : children) {
        delete child;
    }
    for (auto it : components) {
        delete it.second;
	}
}
