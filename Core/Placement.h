#ifndef PLACEMENT_H
#define PLACEMENT_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "rapidjson/document.h"

/*
 * Where a level is put in the world: a point of it ("there", in its own coordinates) goes to a point
 * of the world ("here"), turned round y by "turn" degrees about it. Levels are built each in their own
 * coordinates; one streamed in next to another is placed so their shared door meets:
 *
 *   "attach": {"here": [0, 0, 89.28], "there": [-31.74, 0, 198.4], "turn": -90}
 */
struct Placement {
    glm::quat turn = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 here = glm::vec3(0.0f), there = glm::vec3(0.0f);

    glm::vec3 point(const glm::vec3 &p) const { return turn * (p - there) + here; }
    glm::vec3 direction(const glm::vec3 &d) const { return turn * d; }
    glm::quat rotation(const glm::quat &q) const { return turn * q; }
    bool identity() const { return here == there && turn == glm::quat(1.0f, 0.0f, 0.0f, 0.0f); }
    /* back into the level's own coordinates */
    glm::vec3 inverse(const glm::vec3 &p) const { return glm::inverse(turn) * (p - here) + there; }

    static Placement fromJson(const rapidjson::Value &attach);
};

#endif // PLACEMENT_H
