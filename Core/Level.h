#ifndef LEVEL_H
#define LEVEL_H

#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "rapidjson/document.h"

#include "Interaction.h"
#include "Placement.h"
#include "Player.h"
#include "SoundScape.h"
#include "Timeline.h"

class Entity;
class ParticleEmitter;
class WaterRenderer;
class Light;

/*
 * A level in the world: the first one (the map the engine started on) or one streamed in beside it
 * (Core/LevelStreaming.cpp). What it brought, so it can be taken away again when it cannot be reached
 * any more: its entities, its lights, its sounds, its timeline, what can be done in it.
 */
struct Level {
    std::string path;
    Placement place;
    rapidjson::Document document;       /* its JSON (kept: its stand-ins are built from it later) */
    std::vector<Entity *> entities;
    std::vector<Light *> lights;
    std::string environment;            /* its "Environment" (JSON) */
    glm::vec3 low = glm::vec3(1e9f), high = glm::vec3(-1e9f);   /* its bounds in the world */

    /* its sounds, its timeline, what can be done in it: every level has them, once loaded. The first
     * level's are the engine's own (the timeline editor and the aftermath follow its timeline); a
     * streamed level's are its own (below) */
    SoundScape *sounds = nullptr;
    Timeline *timeline = nullptr;
    Interaction *interaction = nullptr;
    std::unique_ptr<SoundScape> ownSounds;
    std::unique_ptr<Timeline> ownTimeline;
    std::unique_ptr<Interaction> ownInteraction;
    float streamedAt = 0.0f;            /* the first level's timeline clock when it came (seeking puts its
                                         * own clock as far after that) */

    /* what it brought into the world beside its entities and lights */
    std::vector<ParticleEmitter *> emitters;    /* its particle emitters (its footsteps' dust too) */
    WaterRenderer *water = nullptr;             /* its water (one in the world: the first level that has it) */
    Player::Footsteps footsteps;                /* how the player's steps sound in it */

    bool first = false;                 /* the map the engine started on */
    /* streaming in: how far it has got */
    bool loaded = false;
    size_t nextEntity = 0, nextLight = 0;
    /* stand-ins: entities of it inside these boxes (its own coordinates) are built only once the level
     * that streamed it in is gone (its own door, where that level's door stood) */
    std::string streamedBy;
    std::vector<std::pair<glm::vec3, glm::vec3>> standInBoxes;
    std::vector<size_t> standIns;       /* (indices into its "Entities") */
    std::vector<std::string> pictures;  /* its textures, decoded ahead on worker threads */
    std::vector<std::string> models;    /* its model files, parsed ahead on worker threads */
    bool unloadWanted = false;

    bool contains(const glm::vec3 &p, float margin = 0.0f) const {
        return glm::all(glm::greaterThanEqual(p, low - glm::vec3(margin))) &&
               glm::all(glm::lessThanEqual(p, high + glm::vec3(margin)));
    }
};

#endif // LEVEL_H
