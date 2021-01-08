#ifndef _COMMON_H_
#define _COMMON_H_

#include "Entity.h"
#include "Light.h"
#include "QuadTree.hpp"

enum class PointerType {
    ENTITY = 0,
    LIGHT,
    QUADT_NODE
};

union Pointer {
    Entity *entity;
    Light *light;
    QuadTree::Node *node;
};

struct UserData {
    PointerType type;
    Pointer pointer;
};

#endif /* _COMMON_H_ */
