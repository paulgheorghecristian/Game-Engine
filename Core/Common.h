#ifndef _COMMON_H_
#define _COMMON_H_

#include "Entity.h"
#include "Light.h"

enum class PointerType {
    ENTITY = 0,
    LIGHT
};

union Pointer {
    Entity *entity;
    Light *light;
};

struct UserData {
    PointerType type;
    Pointer pointer;
};

#endif /* _COMMON_H_ */