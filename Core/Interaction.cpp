#include "Interaction.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

#include "AudioMaster.h"
#include "Camera.h"
#include "DrivenComponent.h"
#include "Entity.h"
#include "Input.h"
#include "PhysicsMaster.h"
#include "PressableComponent.h"
#include "Timeline.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

bool Interaction::s_holding = false;

static glm::vec3 readVec3(const rapidjson::Value &json, glm::vec3 fallback = glm::vec3(0.0f))
{
    if (!json.IsArray() || json.Size() != 3 || !json[0].IsNumber() || !json[1].IsNumber() || !json[2].IsNumber()) {
        return fallback;
    }
    return glm::vec3(json[0].GetFloat(), json[1].GetFloat(), json[2].GetFloat());
}

static float readFloat(const rapidjson::Value &json, const char *key, float fallback)
{
    return json.IsObject() && json.HasMember(key) && json[key].IsNumber() ? json[key].GetFloat() : fallback;
}

static std::string readString(const rapidjson::Value &json, const char *key, const std::string &fallback = "")
{
    return json.IsObject() && json.HasMember(key) && json[key].IsString() ? json[key].GetString() : fallback;
}

/* the sounds a list of actions plays, loaded now (not on the first press: a hitch) */
static void preload(const rapidjson::Value &actions)
{
    if (!actions.IsArray()) return;
    for (const auto &action : actions.GetArray()) {
        std::string sound = readString(action, "sound");
        if (!sound.empty()) AudioMaster::getInstance()->load(sound);
    }
}

void Interaction::load(const rapidjson::Value &map, const std::vector<Entity *> &entities, const Placement &place)
{
    m_place = place;
    m_reactions.clear();
    m_requests.clear();
    m_aimed = nullptr;
    rapidjson::Document().Swap(m_document);     /* (a fresh one: SetObject kept the old pool's memory) */
    m_document.SetObject();
    auto &allocator = m_document.GetAllocator();
    m_locks.clear();
    m_triggers.clear();
    m_pressables.clear();
    m_driven.clear();
    m_events.clear();
    m_json.clear();
    /* (s_holding: the player is held by whichever level is leaving; one level loading must not free
     * them while another fades out) */

    if (map.IsObject() && map.HasMember("Interaction") && map["Interaction"].IsObject()) {
        const rapidjson::Value &block = map["Interaction"];
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        block.Accept(writer);
        m_json = buffer.GetString();

        if (block.HasMember("locks") && block["locks"].IsArray()) {
            for (const auto &json : block["locks"].GetArray()) {
                if (!json.IsObject()) continue;
                Lock lock;
                lock.name = readString(json, "name");
                lock.code = readString(json, "code");
                lock.clear = readString(json, "clear", lock.clear);
                lock.enter = readString(json, "enter", lock.enter);
                lock.onSolved.SetArray();
                lock.onWrong.SetArray();
                if (json.HasMember("onSolved")) lock.onSolved.CopyFrom(json["onSolved"], allocator);
                if (json.HasMember("onWrong")) lock.onWrong.CopyFrom(json["onWrong"], allocator);
                preload(lock.onSolved);
                preload(lock.onWrong);
                if (lock.name.empty() || lock.code.empty()) {
                    std::cout << "Interaction: a lock without a name or a code skipped" << std::endl;
                    continue;
                }
                m_locks.push_back(std::move(lock));
            }
        }
        if (block.HasMember("triggers") && block["triggers"].IsArray()) {
            for (const auto &json : block["triggers"].GetArray()) {
                if (!json.IsObject() || !json.HasMember("box") || !json["box"].IsArray() || json["box"].Size() != 2) {
                    std::cout << "Interaction: a trigger without a \"box\" ([[low], [high]]) skipped" << std::endl;
                    continue;
                }
                Trigger trigger;
                glm::vec3 a = readVec3(json["box"][0]), b = readVec3(json["box"][1]);
                /* (where the level is in the world: the box round its turned corners) */
                trigger.low = glm::vec3(1e9f);
                trigger.high = glm::vec3(-1e9f);
                for (int corner = 0; corner < 8; corner++) {
                    glm::vec3 c((corner & 1) ? a.x : b.x, (corner & 2) ? a.y : b.y, (corner & 4) ? a.z : b.z);
                    trigger.low = glm::min(trigger.low, place.point(c));
                    trigger.high = glm::max(trigger.high, place.point(c));
                }
                trigger.after = readString(json, "after");
                trigger.once = !(json.HasMember("once") && json["once"].IsBool() && !json["once"].GetBool());
                trigger.on.SetArray();
                if (json.HasMember("on")) trigger.on.CopyFrom(json["on"], allocator);
                preload(trigger.on);
                m_triggers.push_back(std::move(trigger));
            }
        }
        if (block.HasMember("reactions") && block["reactions"].IsArray()) {
            for (const auto &json : block["reactions"].GetArray()) {
                std::string event = readString(json, "event");
                if (event.empty() || !json.HasMember("on")) continue;
                rapidjson::Value on;
                on.CopyFrom(json["on"], allocator);
                preload(on);
                m_reactions.emplace_back(event, std::move(on));
            }
        }
    }

    for (Entity *entity : entities) adopt(entity);
}

void Interaction::adopt(Entity *entity)
{
    if (PressableComponent *pressable = (PressableComponent *) entity->getComponent(Entity::Flags::PRESSABLE)) {
        if (!pressable->placed) {
            pressable->travel = m_place.direction(pressable->travel);   /* (turned with its level) */
            pressable->placed = true;
        }
        m_pressables.push_back(pressable);
        preload(pressable->actions());
    }
    if (DrivenComponent *driven = (DrivenComponent *) entity->getComponent(Entity::Flags::DRIVEN)) {
        if (!driven->placed) {
            /* its hinge and how it turns and moves, where its level is in the world */
            driven->pivot = m_place.point(driven->pivot);
            driven->rotate = m_place.direction(driven->rotate);
            driven->move = m_place.direction(driven->move);
            driven->placed = true;
        }
        m_driven.push_back(driven);
    }
}

void Interaction::forget(Entity *entity)
{
    m_pressables.erase(std::remove_if(m_pressables.begin(), m_pressables.end(),
                                      [entity](PressableComponent *p) { return p->getEntity() == entity; }),
                       m_pressables.end());
    m_driven.erase(std::remove_if(m_driven.begin(), m_driven.end(),
                                  [entity](DrivenComponent *d) { return d->getEntity() == entity; }),
                   m_driven.end());
    if (m_aimed != nullptr && m_aimed->getEntity() == entity) m_aimed = nullptr;
}

float Interaction::aimedDistance() const
{
    return m_aimed != nullptr ? m_aimedAt : 1e9f;
}

void Interaction::yieldAim()
{
    m_aimed = nullptr;
    m_aimTarget = 0.0f;
}

bool Interaction::entry(const rapidjson::Value &map, const std::string &name, glm::vec3 &position, float &yaw,
                        float &pitch)
{
    if (!map.IsObject() || !map.HasMember("Interaction") || !map["Interaction"].IsObject()) return false;
    const rapidjson::Value &block = map["Interaction"];
    if (!block.HasMember("entries") || !block["entries"].IsObject() || !block["entries"].HasMember(name.c_str())) {
        return false;
    }
    const rapidjson::Value &entry = block["entries"][name.c_str()];
    if (!entry.IsObject() || !entry.HasMember("position")) return false;
    position = readVec3(entry["position"], position);
    yaw = readFloat(entry, "yaw", 0.0f);
    pitch = readFloat(entry, "pitch", 0.0f);
    return true;
}

void Interaction::arrive(const rapidjson::Value &map, const std::string &name, Timeline &timeline)
{
    const rapidjson::Value &entry = map["Interaction"]["entries"][name.c_str()];
    float fadeIn = readFloat(entry, "fadeIn", 0.0f);
    if (fadeIn > 0.0f) {
        m_fade = 1.0f;
        m_fadeFrom = 1.0f;
        m_fadeTo = 0.0f;
        m_fadeLength = fadeIn;
        m_fadeStart = -1.0f;                    /* (from the first frame: loading takes a while) */
    }
    std::string event = readString(entry, "event");
    if (!event.empty()) signal(event, timeline);
    if (entry.HasMember("on")) {
        preload(entry["on"]);
        run(entry["on"], nullptr, timeline, 0.0f);
    }
}

void Interaction::signal(const std::string &event, Timeline &timeline)
{
    m_events.insert(event);
    timeline.signal(event);
    if (m_signalDepth > 8) return;
    m_signalDepth++;
    for (auto &reaction : m_reactions) {
        if (reaction.first == event) run(reaction.second, nullptr, timeline, m_lastFrame);
    }
    m_signalDepth--;
}

glm::vec3 Interaction::placed(const rapidjson::Value &at, const glm::vec3 &fallback) const
{
    if (!at.IsArray() || at.Size() != 3) return fallback;
    return m_place.point(readVec3(at));
}

void Interaction::run(const rapidjson::Value &actions, Entity *source, Timeline &timeline, float time)
{
    if (!actions.IsArray()) return;
    for (const auto &action : actions.GetArray()) {
        if (!action.IsObject()) continue;
        if (action.HasMember("sound") && action["sound"].IsString()) {
            std::string sound = action["sound"].GetString();
            float volume = readFloat(action, "volume", 1.0f), pitch = readFloat(action, "pitch", 1.0f);
            bool spatial = action.HasMember("at") || source != nullptr;
            if (spatial) {
                glm::vec3 at = source != nullptr ? source->getTransform().getPosition() : glm::vec3(0.0f);
                if (source != nullptr) {
                    /* (a pressed thing's middle: its mesh may be placed in the world, at an origin of 0) */
                    PressableComponent *pressable = (PressableComponent *) source->getComponent(Entity::Flags::PRESSABLE);
                    glm::vec3 low, high;
                    if (pressable != nullptr && pressable->localBounds(low, high)) {
                        at = glm::vec3(source->getTransform().getModelMatrix() * glm::vec4((low + high) * 0.5f, 1.0f));
                    }
                }
                if (action.HasMember("at")) at = placed(action["at"], at);
                AudioMaster::getInstance()->play3D(sound, at, volume, readFloat(action, "radius", 300.0f), pitch);
            } else {
                AudioMaster::getInstance()->play(sound, volume, pitch);
            }
        }
        if (action.HasMember("lock") && action["lock"].IsString()) {
            key(action["lock"].GetString(), readString(action, "key"), timeline, time);
        }
        if (action.HasMember("event") && action["event"].IsString()) {
            signal(action["event"].GetString(), timeline);
        }
        if (action.HasMember("streamLevel") && action["streamLevel"].IsString()) {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            action.Accept(writer);
            m_requests.push_back({"stream", buffer.GetString()});
        }
        if (action.HasMember("unloadLevel") && action["unloadLevel"].IsString()) {
            m_requests.push_back({"unload", action["unloadLevel"].GetString()});
        }
        if (action.HasMember("loadLevel") && action["loadLevel"].IsString() && m_levelPath.empty()) {
            m_levelPath = action["loadLevel"].GetString();
            m_levelEntry = readString(action, "entry");
            m_fadeFrom = std::max(m_fade, 0.0f);
            m_fadeTo = 1.0f;
            m_fadeLength = std::max(readFloat(action, "fade", 0.9f), 0.01f);
            m_fadeStart = time;
            m_fade = m_fadeFrom;
            s_holding = true;
        }
    }
}

void Interaction::key(const std::string &lockName, const std::string &key, Timeline &timeline, float time)
{
    for (Lock &lock : m_locks) {
        if (lock.name != lockName) continue;
        if (lock.solved) return;                /* (open: the keys still beep, nothing more) */
        if (key == lock.clear) {
            lock.typed.clear();
            return;
        }
        if (key != lock.enter) lock.typed += key;
        if (key == lock.enter || lock.typed.size() >= lock.code.size()) {
            bool right = lock.typed == lock.code;
            lock.typed.clear();
            if (right) {
                lock.solved = true;
                run(lock.onSolved, nullptr, timeline, time);
            } else {
                run(lock.onWrong, nullptr, timeline, time);
            }
        }
        return;
    }
    std::cout << "Interaction: no lock \"" << lockName << "\"" << std::endl;
}

void Interaction::input(Input &input, Timeline &timeline, float time)
{
    /* (only while looking round: with the cursor free a click picks things in the editor) */
    if (s_holding || m_aimed == nullptr || !input.getWarpMouse() || !input.getMouseDown(SDL_BUTTON_LEFT)) return;
    m_aimed->pressedAt = time;
    m_pressedAt = time;
    run(m_aimed->actions(), m_aimed->getEntity(), timeline, time);
}

namespace {
/* the first solid thing on the aim (not the player, not a ghost that only senses) */
struct SolidOnly : public btCollisionWorld::ClosestRayResultCallback {
    const btCollisionObject *m_skip;
    SolidOnly(const btVector3 &from, const btVector3 &to, const btCollisionObject *skip)
        : btCollisionWorld::ClosestRayResultCallback(from, to), m_skip(skip) {}
    bool needsCollision(btBroadphaseProxy *proxy) const override {
        const btCollisionObject *object = (const btCollisionObject *) proxy->m_clientObject;
        if (object == m_skip || (object->getCollisionFlags() & btCollisionObject::CF_NO_CONTACT_RESPONSE)) return false;
        return btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy);
    }
};
}

void Interaction::frame(Camera *camera, Timeline &timeline, float time, btRigidBody *playerBody)
{
    float dt = m_lastFrame < 0.0f ? 0.0f : std::max(time - m_lastFrame, 0.0f);
    m_lastFrame = time;

    /* what the level's timeline fired this frame: events like any other (reactions) */
    for (const std::string &event : timeline.firedList()) {
        if (!m_events.count(event)) signal(event, timeline);
    }

    /* what is aimed at: the nearest pressable the middle of the screen is on */
    const glm::vec3 eye = camera->getPosition(), direction = glm::normalize(camera->getForward());
    PressableComponent *nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (PressableComponent *pressable : m_pressables) {
        glm::vec3 low, high;
        if (!pressable->localBounds(low, high)) continue;
        /* the ray in its own space (t stays the distance in the world) */
        glm::mat4 toLocal = glm::inverse(pressable->getEntity()->getTransform().getModelMatrix());
        glm::vec3 origin = glm::vec3(toLocal * glm::vec4(eye, 1.0f));
        glm::vec3 along = glm::vec3(toLocal * glm::vec4(direction, 0.0f));
        float near = 0.0f, far = std::numeric_limits<float>::max();
        bool hit = true;
        for (int axis = 0; axis < 3 && hit; axis++) {
            if (std::abs(along[axis]) < 1e-8f) {
                hit = origin[axis] >= low[axis] && origin[axis] <= high[axis];
                continue;
            }
            float t0 = (low[axis] - origin[axis]) / along[axis], t1 = (high[axis] - origin[axis]) / along[axis];
            near = std::max(near, std::min(t0, t1));
            far = std::min(far, std::max(t0, t1));
            hit = near <= far;
        }
        if (hit && near < nearestDistance) {
            nearest = pressable;
            nearestDistance = near;
        }
    }
    m_aimed = nullptr;
    m_aimTarget = 0.0f;
    if (nearest != nullptr && nearestDistance <= nearest->reach * 1.6f) {
        /* not through a wall: nothing solid before it */
        glm::vec3 to = eye + direction * nearestDistance;
        SolidOnly blocked(btVector3(eye.x, eye.y, eye.z), btVector3(to.x, to.y, to.z), playerBody);
        PhysicsMaster::getInstance()->getWorld()->rayTest(blocked.m_rayFromWorld, blocked.m_rayToWorld, blocked);
        bool free = !blocked.hasHit() || blocked.m_closestHitFraction * nearestDistance >= nearestDistance - 0.3f;
        if (free) {
            if (nearestDistance <= nearest->reach && !s_holding) {
                m_aimed = nearest;
                m_aimedAt = nearestDistance;
                m_aimTarget = 1.0f;
            } else {
                m_aimTarget = 0.3f;             /* close, not yet within reach */
            }
        }
    }
    m_aimShown += (m_aimTarget - m_aimShown) * (1.0f - std::exp(-dt * 16.0f));

    /* presses going in and coming back */
    for (PressableComponent *pressable : m_pressables) {
        if (pressable->pressedAt < 0.0f) continue;
        Transform &transform = pressable->getEntity()->getTransform();
        if (!pressable->hasRest) {
            pressable->hasRest = true;
            pressable->rest = transform.getPosition();
        }
        float k = (time - pressable->pressedAt) / std::max(pressable->pressTime, 0.01f);
        float depth = k < 0.35f ? k / 0.35f : 1.0f - (k - 0.35f) / 0.65f;
        if (k >= 1.0f) {
            depth = 0.0f;
            pressable->pressedAt = -1.0f;
        }
        transform.setPosition(pressable->rest + pressable->travel * glm::clamp(depth, 0.0f, 1.0f));
    }

    /* walking into places */
    for (Trigger &trigger : m_triggers) {
        if (trigger.once && trigger.done) continue;
        bool inside = glm::all(glm::greaterThanEqual(eye, trigger.low)) && glm::all(glm::lessThanEqual(eye, trigger.high));
        bool armed = trigger.after.empty() || m_events.count(trigger.after) > 0;
        if (inside && armed && !trigger.done) {
            trigger.done = true;
            run(trigger.on, nullptr, timeline, time);
        } else if (!inside && !trigger.once) {
            trigger.done = false;               /* (out again: it can go off on the way back in) */
        }
    }

    /* what the timeline moves */
    for (DrivenComponent *driven : m_driven) {
        driven->apply(timeline.get(driven->param, 0.0f));
        if (!driven->anchor.empty()) timeline.setAnchor(driven->anchor, driven->pivot);
    }

    /* fading out to go, or in on coming */
    if (m_fadeEnded) {
        m_fadeEnded = false;
        m_fade = -1.0f;                         /* (faded in: the picture is the engine's again) */
    } else if (m_fade >= 0.0f) {
        if (m_fadeStart < 0.0f) m_fadeStart = time;
        float k = glm::clamp((time - m_fadeStart) / m_fadeLength, 0.0f, 1.0f);
        k = k * k * (3.0f - 2.0f * k);
        m_fade = m_fadeFrom + (m_fadeTo - m_fadeFrom) * k;
        if (m_fadeTo <= 0.0f && k >= 1.0f) {
            m_fade = 0.0f;                      /* (in: this last frame at 0, then done) */
            m_fadeEnded = true;
        }
    }
}

float Interaction::pressPulse(float time) const
{
    return glm::clamp(1.0f - (time - m_pressedAt) / 0.25f, 0.0f, 1.0f);
}

bool Interaction::levelDue(std::string &path, std::string &entry) const
{
    if (m_levelPath.empty() || m_fade < 0.999f) return false;
    path = m_levelPath;
    entry = m_levelEntry;
    return true;
}
