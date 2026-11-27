/*
 * Levels streamed in and out as the player goes, the engine running on (no loading screen, no
 * restart). A level asks for the next one with a "streamLevel" action (Core/Interaction): it is read,
 * placed beside this one where their doors meet (Core/Placement.h), and built a little each frame
 * (its entities, then its lights, within a few milliseconds a frame), its sounds, its timeline and what
 * can be done in it after. Each level keeps what it brought (Core/Level.h); one that asks to go
 * ("unloadLevel": its door shut behind the player) is taken out once the player is not in it, and the
 * stand-ins of the next one (its own door where this one's stood) are built in its place. Walking from
 * one level into another, the environment (fog, ambient, bloom...) blends from the one to the other.
 */
#include "EngineCore.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

#include "AudioMaster.h"
#include "WaterRenderer.h"
#include "SpotLight.h"
#include "ParticleEmitter.h"
#include "GrassRenderer.h"
#include "Light.h"
#include "PhysicsComponent.h"
#include "RenderComponent.h"
#include "RenderingMaster.h"
#include "RenderingObject.hpp"
#include "Texture.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

static glm::vec3 readPoint(const rapidjson::Value &json, const char *key, glm::vec3 fallback)
{
    if (!json.IsObject() || !json.HasMember(key) || !json[key].IsArray() || json[key].Size() != 3) return fallback;
    const auto &a = json[key];
    return glm::vec3(a[0].GetFloat(), a[1].GetFloat(), a[2].GetFloat());
}

/* every picture a level's JSON names (textures, normal maps...) and every model ("Mesh"), for reading
 * ahead */
static void picturesIn(const rapidjson::Value &value, std::vector<std::string> &out,
                       std::vector<std::string> *models = nullptr)
{
    if (value.IsObject() && models != nullptr && value.HasMember("Mesh") && value["Mesh"].IsString()) {
        models->push_back(value["Mesh"].GetString());
    }
    if (value.IsString()) {
        std::string s = value.GetString();
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        for (const char *ending : {".jpg", ".jpeg", ".png", ".bmp", ".tga"}) {
            size_t n = std::strlen(ending);
            if (lower.size() > n && lower.compare(lower.size() - n, n, ending) == 0) { out.push_back(s); break; }
        }
    } else if (value.IsArray()) {
        for (const auto &v : value.GetArray()) picturesIn(v, out, models);
    } else if (value.IsObject()) {
        for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it) picturesIn(it->value, out, models);
    }
}

static std::string toJson(const rapidjson::Value &value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

Placement Placement::fromJson(const rapidjson::Value &attach)
{
    Placement place;
    if (!attach.IsObject()) return place;
    place.here = readPoint(attach, "here", place.here);
    place.there = readPoint(attach, "there", place.there);
    float turn = attach.HasMember("turn") && attach["turn"].IsNumber() ? attach["turn"].GetFloat() : 0.0f;
    place.turn = glm::angleAxis(glm::radians(turn), glm::vec3(0.0f, 1.0f, 0.0f));
    return place;
}

/* a level's bounds grown by what an entity takes up (its meshes' own box, turned and placed: a long
 * wall's bounding sphere would reach far into the next room) */
static void extendBounds(Level &level, Entity *entity)
{
    RenderComponent *render = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
    if (render == nullptr) return;
    glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
    bool any = false;
    for (Mesh *mesh : render->getRenderingObject().getMeshes()) {
        for (const Vertex &v : mesh->getVertices()) {
            lo = glm::min(lo, v.positionCoords);
            hi = glm::max(hi, v.positionCoords);
            any = true;
        }
    }
    if (!any) return;
    const glm::mat4 &model = entity->getTransform().getModelMatrix();
    for (int corner = 0; corner < 8; corner++) {
        glm::vec3 c((corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z);
        glm::vec3 w = glm::vec3(model * glm::vec4(c, 1.0f));
        level.low = glm::min(level.low, w);
        level.high = glm::max(level.high, w);
    }
}

void EngineCore::adoptFirstLevel(rapidjson::Document &gameDocument, size_t firstLight)
{
    std::unique_ptr<Level> level(new Level());
    level->path = m_mapPath;
    level->first = true;
    level->loaded = true;
    level->entities = entities;
    for (Entity *entity : entities) extendBounds(*level, entity);
    std::vector<Light *> &lights = RenderingMaster::getInstance()->getLights();
    for (size_t i = firstLight; i < lights.size(); i++) level->lights.push_back(lights[i]);
    if (gameDocument.HasMember("Environment")) level->environment = toJson(gameDocument["Environment"]);
    if (gameDocument.HasMember("Entities")) {
        std::vector<std::string> pictures;     /* (its models: let go when it is taken out) */
        picturesIn(gameDocument["Entities"], pictures, &level->models);
    }
    /* its sounds, timeline, interaction: the engine's own; what it brought into the world */
    level->sounds = &m_soundScape;
    level->timeline = &m_timeline;
    level->interaction = &m_interaction;
    level->emitters = RenderingMaster::getInstance()->particleEmitters;
    level->water = RenderingMaster::getInstance()->water;
    level->footsteps = Player::footsteps();
    m_zone = level.get();
    m_envTo = level->environment;
    m_levels.push_back(std::move(level));
}

/* what a streamed level brings into the world beside its entities, lights, sounds and timeline, placed
 * where it is put: its particle emitters, its footsteps (heard while the player is in it), its water.
 * What there is one of in the world and belongs to the map started on (its grass, its waking-up
 * scene, the room coming alive, the sky's black holes) is not streamed: said, not dropped silently */
void EngineCore::streamFeatures(Level &level)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    const rapidjson::Document &doc = level.document;
    if (doc.HasMember("ParticleEmitters") && doc["ParticleEmitters"].IsArray()) {
        for (const auto &source : doc["ParticleEmitters"].GetArray()) {
            if (!source.IsObject()) continue;
            rapidjson::Document json;
            json.CopyFrom(source, json.GetAllocator());
            auto place = [&](rapidjson::Value &v, bool point) {
                if (!v.IsArray() || v.Size() < 3) return;
                glm::vec3 p(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat());
                p = point ? level.place.point(p) : level.place.direction(p);
                v[0].SetFloat(p.x); v[1].SetFloat(p.y); v[2].SetFloat(p.z);
            };
            if (json.HasMember("position")) place(json["position"], true);
            if (json.HasMember("direction")) place(json["direction"], false);
            if (json.HasMember("positions") && json["positions"].IsArray()) {
                for (auto &p : json["positions"].GetArray()) place(p, true);
            }
            ParticleEmitter *emitter = new ParticleEmitter(json);
            /* a beam linked to a light: one of this level's */
            int index = emitter->getLightIndex();
            if (index >= 0 && index < (int) level.lights.size()) {
                if (SpotLight *spot = dynamic_cast<SpotLight *>(level.lights[index])) emitter->linkLight(spot);
            }
            rm->addParticleEmitter(emitter);
            level.emitters.push_back(emitter);
        }
    }
    if (doc.HasMember("Footsteps")) {
        level.footsteps = readFootsteps(doc["Footsteps"]);
        if (level.footsteps.dust != nullptr) level.emitters.push_back(level.footsteps.dust);
    }
    if (doc.HasMember("Water") && doc["Water"].IsObject()) {
        if (rm->water != nullptr) {
            std::cout << level.path << ": its Water not streamed in (one water in the world, and there is one)" << std::endl;
        } else {
            rapidjson::Document json;
            json.CopyFrom(doc["Water"], json.GetAllocator());
            const rapidjson::Value &w = doc["Water"];
            float height = w.HasMember("height") && w["height"].IsNumber() ? w["height"].GetFloat() : 0.0f;
            glm::vec2 center(0.0f), size(1000.0f);
            if (w.HasMember("center") && w["center"].IsArray() && w["center"].Size() >= 2) center = glm::vec2(w["center"][0].GetFloat(), w["center"][1].GetFloat());
            if (w.HasMember("size") && w["size"].IsArray() && w["size"].Size() >= 2) size = glm::vec2(w["size"][0].GetFloat(), w["size"][1].GetFloat());
            glm::vec3 c = level.place.point(glm::vec3(center.x, height, center.y));
            /* the water is a box along the world's axes: turned, it covers the turned box's extent */
            glm::vec3 across = level.place.direction(glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec2 placedSize(std::abs(across.x) * size.x + std::abs(across.z) * size.y,
                                 std::abs(across.z) * size.x + std::abs(across.x) * size.y);
            if (std::abs(across.x) > 1e-3f && std::abs(across.z) > 1e-3f) {
                std::cout << level.path << ": its Water turned by other than a right angle covers its turned box" << std::endl;
            }
            auto &a = json.GetAllocator();
            json.RemoveMember("height"); json.RemoveMember("center"); json.RemoveMember("size");
            rapidjson::Value centerJson(rapidjson::kArrayType), sizeJson(rapidjson::kArrayType);
            centerJson.PushBack(c.x, a).PushBack(c.z, a);
            sizeJson.PushBack(placedSize.x, a).PushBack(placedSize.y, a);
            json.AddMember("height", c.y, a);
            json.AddMember("center", centerJson, a);
            json.AddMember("size", sizeJson, a);
            rm->water = new WaterRenderer(json, rm->getRenderWidth(), rm->getRenderHeight());
            level.water = rm->water;
        }
    }
    for (const char *key : {"Grass", "Waking", "RoomAlive", "BlackHoles"}) {
        if (doc.HasMember(key)) {
            std::cout << level.path << ": its " << key << " not streamed in (the map started on has the world's)" << std::endl;
        }
    }
}

void EngineCore::streamLevel(const std::string &requestJson, Level *from)
{
    rapidjson::Document request;
    if (request.Parse(requestJson.c_str()).HasParseError() || !request.HasMember("streamLevel")) return;
    std::string path = request["streamLevel"].GetString();
    for (auto &level : m_levels) {
        if (level->path == path) return;                    /* (already in the world) */
    }
    std::ifstream file(path);
    if (!file) {
        std::cout << "streamLevel: " << path << " does not exist" << std::endl;
        return;
    }
    std::stringstream text;
    text << file.rdbuf();
    std::unique_ptr<Level> level(new Level());
    if (level->document.Parse(text.str().c_str()).HasParseError() || !level->document.IsObject()) {
        std::cout << "streamLevel: could not parse " << path << std::endl;
        return;
    }
    level->path = path;
    static const rapidjson::Value nowhere;
    level->place = Placement::fromJson(request.HasMember("attach") ? request["attach"] : nowhere);
    level->streamedBy = from != nullptr ? from->path : std::string();
    if (request.HasMember("standIns") && request["standIns"].IsArray()) {
        for (const auto &box : request["standIns"].GetArray()) {
            if (!box.IsArray() || box.Size() != 2) continue;
            glm::vec3 a(box[0][0].GetFloat(), box[0][1].GetFloat(), box[0][2].GetFloat());
            glm::vec3 b(box[1][0].GetFloat(), box[1][1].GetFloat(), box[1][2].GetFloat());
            level->standInBoxes.emplace_back(glm::min(a, b), glm::max(a, b));
        }
    }
    if (level->document.HasMember("Entities") && level->document["Entities"].IsArray()) {
        const auto &all = level->document["Entities"];
        for (rapidjson::SizeType i = 0; i < all.Size(); i++) {
            glm::vec3 p = readPoint(all[i]["Transform"], "position", glm::vec3(0.0f));
            for (auto &box : level->standInBoxes) {
                if (glm::all(glm::greaterThanEqual(p, box.first)) && glm::all(glm::lessThanEqual(p, box.second))) {
                    level->standIns.push_back(i);
                    break;
                }
            }
        }
    }
    if (level->document.HasMember("Environment")) level->environment = toJson(level->document["Environment"]);
    /* its pictures read and decoded on worker threads while the game runs on; its entities are built
     * once they are ready (a big texture decoded on the game's thread stalled it a third of a second) */
    if (level->document.HasMember("Entities")) {
        picturesIn(level->document["Entities"], level->pictures, &level->models);
        for (auto *list : {&level->pictures, &level->models}) {
            std::sort(list->begin(), list->end());
            list->erase(std::unique(list->begin(), list->end()), list->end());
        }
        for (const std::string &picture : level->pictures) Texture::preload(picture);
        for (const std::string &model : level->models) RenderingObject::preload(model);
    }
    std::cout << "Streaming in " << path << " (" << level->standIns.size() << " stand-ins wait for "
              << (level->streamedBy.empty() ? std::string("nothing") : level->streamedBy) << ")" << std::endl;
    m_levels.push_back(std::move(level));
}

void EngineCore::streamStep(float time)
{
    using Clock = std::chrono::steady_clock;
    const auto budgetEnd = Clock::now() + std::chrono::microseconds(2500);  /* a slice a frame: no hitch */

    for (auto &owned : m_levels) {
        Level &level = *owned;
        if (level.loaded) continue;
        rapidjson::Document &doc = level.document;
        for (const std::string &picture : level.pictures) {
            if (!Texture::ready(picture)) return;            /* (still decoding) */
        }
        for (const std::string &model : level.models) {
            if (!RenderingObject::ready(model)) return;      /* (still parsing) */
        }
        /* its entities, then its lights */
        if (doc.HasMember("Entities") && doc["Entities"].IsArray()) {
            const auto &all = doc["Entities"];
            while (level.nextEntity < all.Size() && Clock::now() < budgetEnd) {
                size_t i = level.nextEntity++;
                if (std::find(level.standIns.begin(), level.standIns.end(), i) != level.standIns.end()) continue;
                Entity *entity = createEntity(all[(rapidjson::SizeType) i], level.place);
                entities.push_back(entity);
                level.entities.push_back(entity);
                extendBounds(level, entity);
            }
            if (level.nextEntity < all.Size()) return;
        }
        if (doc.HasMember("Lights") && doc["Lights"].IsArray()) {
            const auto &all = doc["Lights"];
            while (level.nextLight < all.Size() && Clock::now() < budgetEnd) {
                Light *light = createLight(all[(rapidjson::SizeType) level.nextLight++], level.place);
                if (light != nullptr) level.lights.push_back(light);
            }
            if (level.nextLight < all.Size()) return;
        }
        /* its sounds (placed), its timeline, what can be done in it */
        level.ownSounds.reset(new SoundScape());
        level.sounds = level.ownSounds.get();
        if (doc.HasMember("Audio") && doc["Audio"].IsArray()) {
            rapidjson::Document audio;
            audio.CopyFrom(doc["Audio"], audio.GetAllocator());
            for (auto &sound : audio.GetArray()) {
                if (!sound.IsObject() || !sound.HasMember("position") || !sound["position"].IsArray()) continue;
                glm::vec3 p = level.place.point(readPoint(sound, "position", glm::vec3(0.0f)));
                sound["position"][0].SetFloat(p.x);
                sound["position"][1].SetFloat(p.y);
                sound["position"][2].SetFloat(p.z);
            }
            level.sounds->load(audio);
        }
        level.ownTimeline.reset(new Timeline());
        level.timeline = level.ownTimeline.get();
        if (doc.HasMember("Sequences")) level.timeline->load(doc["Sequences"]);
        level.streamedAt = m_timeline.clock();
        level.ownInteraction.reset(new Interaction());
        level.interaction = level.ownInteraction.get();
        level.interaction->load(doc, level.entities, level.place);
        streamFeatures(level);
        level.loaded = true;
        /* what was read ahead and not used (a cache hit under another name, a picture only stand-ins
         * use): freed, not kept for ever */
        for (const std::string &picture : level.pictures) Texture::dropPreloaded(picture);
        for (const std::string &model : level.models) RenderingObject::dropPreloaded(model);
        /* the level that held its stand-ins back already gone: build them now */
        bool waiting = false;
        for (auto &other : m_levels) waiting |= other->path == level.streamedBy && other.get() != &level;
        if (!waiting) buildStandIns(level);
        std::cout << "Streamed in " << level.path << ": " << level.entities.size() << " entities, "
                  << level.lights.size() << " lights, " << level.emitters.size() << " emitters"
                  << (level.water != nullptr ? ", water" : "") << std::endl;
        return;
    }
    (void) time;
}

void EngineCore::updateLevelsInput()
{
    float time = RenderingMaster::getInstance()->getTime();
    for (auto &level : m_levels) {
        if (level->loaded && level->interaction) level->interaction->input(inputManager, *level->timeline, time);
    }
}

/* once a tick (not while a frame is drawn: taking entities out then would leave the interpolation
 * holding them): what the levels ask for, a level going, a slice of one coming, which level the
 * player is in */
void EngineCore::updateLevels(float time)
{
    Camera *camera = RenderingMaster::getInstance()->getCamera();
    /* what the levels ask for: another level streamed in, themselves taken out (gathered first:
     * streaming one in adds to the list being gone through) */
    std::vector<std::pair<Level *, Interaction::Request>> requests;
    for (auto &owned : m_levels) {
        Level *level = owned.get();
        if (level->interaction == nullptr) continue;
        for (Interaction::Request &request : level->interaction->takeRequests()) requests.emplace_back(level, request);
    }
    for (auto &request : requests) {
        if (request.second.kind == "stream") streamLevel(request.second.json, request.first);
        else if (request.second.kind == "unload") request.first->unloadWanted = true;
    }
    /* a level that asked to go, once the player is not in it (its door is shut behind them) */
    glm::vec3 eye = camera->getPosition();
    for (size_t i = 0; i < m_levels.size(); i++) {
        if (m_levels[i]->unloadWanted && m_levels[i]->loaded && !m_levels[i]->contains(eye, 2.0f)) {
            unloadLevel(m_levels[i].get());
            break;                              /* (one a tick; the list has changed) */
        }
    }
    streamStep(time);
    updateZone(time);
}

/* once a frame: the streamed levels' own timelines, what is aimed at in them, their sounds */
void EngineCore::updateLevelsFrame(float time)
{
    Camera *camera = RenderingMaster::getInstance()->getCamera();
    PhysicsComponent *body = player != NULL ? (PhysicsComponent *) player->getComponent(Entity::Flags::DYNAMIC) : NULL;
    glm::vec3 eye = camera->getPosition();
    const bool paused = RenderingMaster::getInstance()->isWorldPaused();
    /* big textures going up in strips (a frame's share; whichever level they are for) */
    Texture::pumpUploads(2.0);
    for (auto &level : m_levels) {
        if (!level->loaded) continue;
        /* (the first level's timeline is the engine's: advanced by it, the aftermath following it) */
        if (level->ownTimeline && !paused) level->timeline->advance(time);
        level->interaction->frame(camera, *level->timeline, time, body != NULL ? body->getRigidBody() : NULL);
        if (level->ownTimeline) level->timeline->playSounds(eye);
        /* a level leaving the old way (a fade and another map) */
        Interaction &interaction = *level->interaction;
        if (interaction.fade() >= 0.0f) {
            RenderingMaster::getInstance()->setFadeOut(interaction.fade());
            /* the sound too when leaving; coming in, only the picture (the door shut behind is heard) */
            if (Interaction::holdsPlayer()) AudioMaster::getInstance()->setFade(1.0f - interaction.fade());
        }
        std::string path, entry;
        if (interaction.levelDue(path, entry)) loadLevel(path, entry);
    }
    /* one click presses one thing: the nearest aimed at, of all the levels */
    Interaction *nearest = nullptr;
    for (auto &level : m_levels) {
        if (level->loaded && level->interaction &&
            (nearest == nullptr || level->interaction->aimedDistance() < nearest->aimedDistance())) {
            nearest = level->interaction;
        }
    }
    for (auto &level : m_levels) {
        if (level->interaction && level->interaction != nearest) level->interaction->yieldAim();
    }
}

void EngineCore::buildStandIns(Level &level)
{
    if (level.standIns.empty()) return;
    const auto &all = level.document["Entities"];
    for (size_t i : level.standIns) {
        Entity *entity = createEntity(all[(rapidjson::SizeType) i], level.place);
        entities.push_back(entity);
        level.entities.push_back(entity);
        extendBounds(level, entity);
        if (level.interaction) level.interaction->adopt(entity);
    }
    level.standIns.clear();
    level.streamedBy.clear();
}

void EngineCore::forgetEntity(Entity *entity)
{
    for (auto &level : m_levels) {
        auto at = std::find(level->entities.begin(), level->entities.end(), entity);
        if (at == level->entities.end()) continue;
        level->entities.erase(at);
        if (level->interaction != nullptr) level->interaction->forget(entity);
    }
}

void EngineCore::forgetLight(Light *light)
{
    for (auto &level : m_levels) {
        level->lights.erase(std::remove(level->lights.begin(), level->lights.end(), light), level->lights.end());
    }
}

void EngineCore::unloadLevel(Level *level)
{
    std::cout << "Taking out " << level->path << ": " << level->entities.size() << " entities, "
              << level->lights.size() << " lights, " << level->emitters.size() << " emitters"
              << (level->water != nullptr ? ", water" : "") << std::endl;
    for (Entity *entity : level->entities) {
        auto at = std::find(entities.begin(), entities.end(), entity);
        if (at != entities.end()) entities.erase(at);
        m_previousTick.entities.erase(entity);
        delete entity;
    }
    for (Light *light : level->lights) {
        RenderingMaster::getInstance()->removeLightFromScene(light);
    }
    /* its sounds, its timeline, what could be done in it (a streamed level's go with its record; the
     * first level's are the engine's, emptied) */
    if (level->sounds) level->sounds->clear();
    if (level->timeline) level->timeline->clear();
    if (level->interaction && !level->ownInteraction) {
        rapidjson::Document none;
        none.SetObject();
        level->interaction->load(none, std::vector<Entity *>());
    }
    /* what it brought into the world: its particle emitters (its steps' dust among them), its water */
    RenderingMaster *rm = RenderingMaster::getInstance();
    for (ParticleEmitter *emitter : level->emitters) {
        if (Player::footsteps().dust == emitter) Player::footsteps().dust = nullptr;
        rm->removeParticleEmitter(emitter);
        delete emitter;
    }
    level->emitters.clear();
    if (level->water != nullptr && rm->water == level->water) {
        delete rm->water;
        rm->water = nullptr;
    }
    if (level->first) {
        /* (its grass, which grows on its terrain: the map's "Grass" is the first level's) */
        delete rm->grass;
        rm->grass = nullptr;
    }
    /* what waited for it to go: the next level's own door where this one's stood */
    for (auto &other : m_levels) {
        if (other.get() == level || other->streamedBy != level->path) continue;
        if (other->loaded) buildStandIns(*other);
        else other->streamedBy.clear();     /* (still loading: built as soon as it has) */
    }
    if (m_zone == level) m_zone = nullptr;
    /* its models, unless a level still here uses them too */
    for (const std::string &model : level->models) {
        bool shared = false;
        for (auto &other : m_levels) {
            if (other.get() != level) shared |= std::find(other->models.begin(), other->models.end(), model) != other->models.end();
        }
        if (!shared) RenderingObject::forget(model);
    }
    for (auto at = m_levels.begin(); at != m_levels.end(); ++at) {
        if (at->get() == level) {
            m_levels.erase(at);
            break;
        }
    }
}

/* two environments (as JSON) mixed: numbers and lists of numbers in between, the rest from `to` */
static Environment mixEnvironments(const std::string &fromJson, const std::string &toJson, float k)
{
    rapidjson::Document from, to;
    Environment out;
    if (to.Parse(toJson.c_str()).HasParseError() || !to.IsObject()) return out;
    if (!from.Parse(fromJson.c_str()).HasParseError() && from.IsObject()) {
        for (auto it = to.MemberBegin(); it != to.MemberEnd(); ++it) {
            if (!from.HasMember(it->name)) continue;
            const rapidjson::Value &a = from[it->name];
            rapidjson::Value &b = it->value;
            if (a.IsNumber() && b.IsNumber() && !b.IsBool()) {
                b.SetDouble(a.GetDouble() + (b.GetDouble() - a.GetDouble()) * k);
            } else if (a.IsArray() && b.IsArray() && a.Size() == b.Size()) {
                for (rapidjson::SizeType i = 0; i < b.Size(); i++) {
                    if (a[i].IsNumber() && b[i].IsNumber()) b[i].SetDouble(a[i].GetDouble() + (b[i].GetDouble() - a[i].GetDouble()) * k);
                }
            }
        }
    }
    out.load(to);
    return out;
}

void EngineCore::updateZone(float time)
{
    const float BLEND = 1.5f;
    glm::vec3 eye = RenderingMaster::getInstance()->getCamera()->getPosition();
    if (m_zone == nullptr || !m_zone->contains(eye, 1.0f)) {
        /* which level the player is in now (the smallest that holds them) */
        Level *found = nullptr;
        float smallest = std::numeric_limits<float>::max();
        for (auto &level : m_levels) {
            if (!level->loaded || !level->contains(eye)) continue;
            glm::vec3 size = level->high - level->low;
            float volume = size.x * size.y * size.z;
            if (volume < smallest) { smallest = volume; found = level.get(); }
        }
        if (found != nullptr && found != m_zone) {
            std::cout << "In " << found->path << std::endl;
            /* the player's steps: as this level's floors sound */
            Player::footsteps() = found->footsteps;
            if (!found->environment.empty() && found->environment != m_envTo) {
                /* from where the blend has got to (or the last level's) to this one's */
                if (m_envBlendStart >= 0.0f) {
                    float k = glm::clamp((time - m_envBlendStart) / BLEND, 0.0f, 1.0f);
                    rapidjson::Document now;
                    rapidjson::Value saved;
                    mixEnvironments(m_envFrom, m_envTo, k * k * (3.0f - 2.0f * k)).save(saved, now.GetAllocator());
                    m_envFrom = toJson(saved);
                } else {
                    m_envFrom = m_envTo;
                }
                m_envTo = found->environment;
                m_envBlendStart = time;
            }
            m_zone = found;
        }
    }
    if (m_envBlendStart >= 0.0f) {
        float k = glm::clamp((time - m_envBlendStart) / BLEND, 0.0f, 1.0f);
        Environment mixed = mixEnvironments(m_envFrom, m_envTo, k * k * (3.0f - 2.0f * k));
        /* (the time of day goes on as it is: not each level's own start, nor blended through noon) */
        mixed.timeOfDay = RenderingMaster::getInstance()->getEnvironment().timeOfDay;
        mixed.timePaused = RenderingMaster::getInstance()->getEnvironmentToSave().timePaused;
        RenderingMaster::getInstance()->setEnvironment(mixed);
        if (k >= 1.0f) m_envBlendStart = -1.0f;
    }
}

float EngineCore::aimAcrossLevels() const
{
    float aim = 0.0f;
    for (const auto &level : m_levels) {
        if (level->interaction) aim = std::max(aim, level->interaction->aim());
    }
    return aim;
}

float EngineCore::pulseAcrossLevels(float time) const
{
    float pulse = 0.0f;
    for (const auto &level : m_levels) {
        if (level->interaction) pulse = std::max(pulse, level->interaction->pressPulse(time));
    }
    return pulse;
}
