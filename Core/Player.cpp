#include "ParticleEmitter.h"
#include "Player.h"
#include "Waking.h"
#include "Interaction.h"

#include "RenderingMaster.h"
#include "WaterRenderer.h"
#include "AudioMaster.h"
#include "PhysicsMaster.h"

#include "Common.h"

#define NECK_HEIGHT 10.0f
#define PLAYER_WALK_SPEED 100.0f
#define PLAYER_RUN_SPEED 200.0f
#define DOWN_LENGTH 1000.0f
#define JUMP_SPEED 5600.0f
#define EPS 0.003

#define STEP_FRQ 0.16
#define STEP_AMP 0.35
#define STEP_ROT_FRQ 0.5
#define STEP_ROT_AMP 0.008

Player::Player (Transform &transform) {
    this->m_transform = transform;
    addComponent (new FirstPersonComponent ());
    addComponent (new PlayerControllerComponent ());
}

std::string Player::jsonify() {
    return std::string("");
}

Player::FirstPersonComponent::FirstPersonComponent() {
    m_camera = RenderingMaster::getInstance()->getCamera();
    sineStep = -glm::half_pi<float>();
    dir = true;
    animFOV = false;
    currFOV = 75.0f;
}

const unsigned int Player::FirstPersonComponent::getFlag() const {
    return Entity::Flags::THIRD_PERSON_CAMERA;
}

void Player::FirstPersonComponent::input(Input &inputManager) {
    Player::PlayerControllerComponent *playerController = 
                dynamic_cast<Player::PlayerControllerComponent *> (_entity->getComponent (Entity::Flags::PLAYER_CONTROLLER));

    if (playerController == NULL)
        return;

    float walked = glm::length(glm::vec3(lastPosDiff.x, 0.0f, lastPosDiff.z));

    if (inputManager.getKey(SDLK_LSHIFT) && walked >= 0.45f) {
        currFOV = 85.0f;
        RenderingMaster::getInstance()->setFOV(currFOV);
        playerController->speed = PLAYER_RUN_SPEED;
        animFOV = false;
    }
    if (inputManager.getKeyUp(SDLK_LSHIFT) || (currFOV > 75.0f && walked < 0.45f)) {
        animFOV = true;
        playerController->speed = PLAYER_WALK_SPEED;
    }

    if (animFOV == true) {
        currFOV -= 0.5f;
        if (currFOV <= 75.0f) {
            currFOV = 75.0f;
            animFOV = false;
        }
        RenderingMaster::getInstance()->setFOV(currFOV);

    }
}

void Player::FirstPersonComponent::update() {
    glm::vec3 pos = _entity->getTransform().getPosition();

    lastPosDiff = lastPos - pos;
    float step = (glm::length(glm::vec3(lastPosDiff.x, 0.0f, lastPosDiff.z)))*STEP_FRQ;

    sineStep += (dir == true) ? step : -step;
    if (sineStep > glm::half_pi<float>() && dir) {
        dir = false;
        sineStep = glm::half_pi<float>();
    } else if (sineStep < -glm::half_pi<float>()) {
        dir = true;
        sineStep = -glm::half_pi<float>();
    }

    pos.y += NECK_HEIGHT + glm::sin(sineStep)*STEP_AMP;

    m_camera->setPosition(pos);
    m_camera->setZRot(glm::sin(sineStep*STEP_ROT_FRQ)*STEP_ROT_AMP);

    lastPos = pos;
}

void Player::FirstPersonComponent::render() {

}

void Player::FirstPersonComponent::init() {

}

Player::PlayerControllerComponent::PlayerControllerComponent () : m_isJumping(false), m_wasSpaceReleased(true), m_freeRoam(false) {
    m_camera = RenderingMaster::getInstance()->getCamera();
    speed = PLAYER_WALK_SPEED;
}

void Player::PlayerControllerComponent::input(Input &inputManager) {
    if (Waking::holdsPlayer() || Interaction::holdsPlayer()) {
        return;                                         /* (a waking-up scene: not up yet; leaving a level) */
    }
    glm::vec3 force(0);
    PhysicsComponent *physicsComponent = dynamic_cast<PhysicsComponent *> (_entity->getComponent (Entity::Flags::DYNAMIC));

    if (physicsComponent == NULL) {
        return;
    }

    const glm::vec3 &forward = m_camera->getForward();
    const glm::vec3 &right = m_camera->getRight();

    if (inputManager.getKeyDown(SDLK_f)) {
        m_freeRoam = !m_freeRoam;
    }

    if (inputManager.getKey(SDLK_w)) {
        force += glm::vec3 (forward.x, m_freeRoam ? forward.y : 0, forward.z);
    } else if (inputManager.getKey(SDLK_s)) {
        force -= glm::vec3 (forward.x, m_freeRoam ? forward.y : 0, forward.z);
    }

    if (inputManager.getKey(SDLK_d)) {
        force += glm::vec3 (right.x, m_freeRoam ? right.y : 0, right.z);
    } else if (inputManager.getKey(SDLK_a)) {
        force -= glm::vec3 (right.x, m_freeRoam ? right.y : 0, right.z);
    }

    if (!m_freeRoam) {
        if (inputManager.getKeyDown(SDLK_SPACE)) {
            m_jumpBuffer = 0.15f;   /* used by update() as soon as the player can jump */
        }
    }

    if (glm::length(force) != 0) {
        force = glm::normalize(force) * speed * (m_isJumping ? 0.8f : 1.0f);
        physicsComponent->getRigidBody()->applyCentralForce(btVector3(force.x, m_freeRoam ? force.y : 0, force.z));
    }
}

namespace {
/* Closest hit along a ground probe, ignoring the player's own body and
 * objects that are not walkable geometry (the AI quadtree's ghost boxes,
 * picking proxies). Filtering in needsCollision lets the ray find the real
 * ground behind them instead of stopping at them. */
struct GroundRayCallback : public btCollisionWorld::ClosestRayResultCallback {
    const btCollisionObject *self;
    GroundRayCallback(const btVector3 &from, const btVector3 &to, const btCollisionObject *self)
        : btCollisionWorld::ClosestRayResultCallback(from, to), self(self) {}
    bool needsCollision(btBroadphaseProxy *proxy) const override {
        const btCollisionObject *object = (const btCollisionObject *) proxy->m_clientObject;
        if (object == self) {
            return false;
        }
        const UserData *data = (const UserData *) object->getUserPointer();
        if (data != NULL && (data->type == PointerType::ENTITY || data->type == PointerType::LIGHT ||
                             data->type == PointerType::QUADT_NODE)) {
            return false;
        }
        return btCollisionWorld::ClosestRayResultCallback::needsCollision(proxy);
    }
};

/* the capsule rides this high above the ground (its bottom hovers ~11 units
 * up, so it steps over small obstacles) */
const float RIDE_HEIGHT = 31.0f;
const float GROUNDED_HEIGHT = 36.5f;   /* within this: standing on the ground */
const float PROBE_LENGTH = 120.0f;
const float MAX_WALKABLE_SLOPE = 0.6f; /* ground normal y; steeper: slide, no jump */
const float JUMP_VELOCITY = 29.0f;     /* same jump height as before */
const float TICK = 1.0f / 60.0f;
}

void Player::PlayerControllerComponent::update() {
    PhysicsComponent *physicsComponent = (PhysicsComponent *) _entity->getComponent (Entity::Flags::DYNAMIC);
    if (physicsComponent == NULL) {
        return;
    }
    btRigidBody *body = physicsComponent->getRigidBody();
    btDynamicsWorld *world = PhysicsMaster::getInstance()->getWorld();
    /* walls should not hold the player (pushing into a fence mid-air used to
     * stick to it through friction) or bounce them away */
    body->setFriction(0.0f);
    body->setRestitution(0.0f);

    m_jumpBuffer = std::max(0.0f, m_jumpBuffer - TICK);
    if (m_freeRoam) {
        body->setGravity(btVector3(0, 0, 0));
        return;
    }
    body->setGravity(world->getGravity());

    /* ground probe: five rays down from the capsule (centre and four sides) */
    const glm::vec3 position = _entity->getTransform().getPosition();
    const float offset = physicsComponent->getBoundingBodyScale().x;
    const float offsets[5][2] = { {0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
    float hitY[5];
    glm::vec3 hitNormal[5];
    bool hit[5];
    float groundY = -1e9f;
    for (int i = 0; i < 5; i++) {
        btVector3 from(position.x + offsets[i][0] * offset, position.y, position.z + offsets[i][1] * offset);
        btVector3 to = from - btVector3(0, PROBE_LENGTH, 0);
        GroundRayCallback ray(from, to, body);
        world->rayTest(from, to, ray);
        hit[i] = ray.hasHit();
        if (hit[i]) {
            hitY[i] = ray.m_hitPointWorld.y();
            hitNormal[i] = glm::vec3(ray.m_hitNormalWorld.x(), ray.m_hitNormalWorld.y(), ray.m_hitNormalWorld.z());
            groundY = std::max(groundY, hitY[i]);
        }
    }
    bool hasGround = groundY > -1e8f;
    /* the ground's slope from the rays that agree with the highest hit (one
     * ray catching the edge of a rock no longer decides it alone) */
    glm::vec3 normal(0, 1, 0);
    if (hasGround) {
        glm::vec3 sum(0);
        for (int i = 0; i < 5; i++) {
            if (hit[i] && groundY - hitY[i] < 4.0f) {
                sum += hitNormal[i];
            }
        }
        if (glm::length(sum) > 1e-4f) {
            normal = glm::normalize(sum);
        }
    }

    btVector3 velocity = body->getLinearVelocity();
    float height = position.y - groundY;
    bool walkable = normal.y >= MAX_WALKABLE_SLOPE;
    /* rising from a jump: never snap back down */
    bool risingFromJump = m_isJumping && velocity.y() > 0.0f;
    /* within reach of the ground; right after being on it (and not jumping)
     * the reach is longer, so the player follows the ground over crests and
     * down hills instead of flying off them */
    float reach = (m_groundTimer > 0.0f && !m_isJumping) ? RIDE_HEIGHT + 22.0f : GROUNDED_HEIGHT;
    bool onGround = hasGround && walkable && !risingFromJump && height <= reach;

    if (onGround) {
        m_isJumping = false;
        m_groundTimer = 0.12f;
        /* ride height set directly (as the original controller did), a few
         * units per tick at most so the capsule is never shoved into
         * geometry, and no vertical speed is kept: nothing launches the
         * player at the top of a slope */
        float dy = glm::clamp((groundY + RIDE_HEIGHT) - position.y, -8.0f, 8.0f);
        btTransform transform = body->getCenterOfMassTransform();
        transform.setOrigin(transform.getOrigin() + btVector3(0, dy, 0));
        body->setCenterOfMassTransform(transform);
        /* slopes do not add speed: grounded speed is capped at the flat
         * ground walking / running speed */
        btVector3 horizontal(velocity.x(), 0, velocity.z());
        float maxSpeed = speed * 0.095f;
        if (horizontal.length() > maxSpeed) {
            horizontal *= maxSpeed / horizontal.length();
        }
        body->setLinearVelocity(btVector3(horizontal.x(), 0, horizontal.z()));
        body->setDamping(btScalar(0.4f + (1.1f - normal.y)), btScalar(0));
    } else {
        m_groundTimer = std::max(0.0f, m_groundTimer - TICK);
        /* in the air or on a too steep slope: gravity takes over (it slides
         * down slopes instead of getting trapped on them) */
        body->setDamping(btScalar(0.2), btScalar(0));
    }

    playSounds(onGround, groundY, position);

    /* jump: buffered press + coyote time, a fixed take-off velocity */
    if (m_jumpBuffer > 0.0f && m_groundTimer > 0.0f) {
        jump(physicsComponent);
        m_jumpBuffer = 0.0f;
        m_groundTimer = 0.0f;
    }
}

/* footsteps paced by the distance walked (longer strides when running),
 * wading steps in water, a splash on going in, a thump on landing */
Player::Footsteps &Player::footsteps() {
    static Footsteps footsteps;
    return footsteps;
}

void Player::PlayerControllerComponent::playSounds(bool onGround, float groundY, const glm::vec3 &position) {
    AudioMaster *audio = AudioMaster::getInstance();
    const Footsteps &steps = footsteps();
    glm::vec3 moved = position - m_lastSoundPosition;
    float horizontal = glm::length(glm::vec2(moved.x, moved.z));
    float perSecond = horizontal / TICK;
    if (!m_soundsStarted || horizontal > 50.0f) {
        /* first tick, or a teleport (respawn, map reload): no sounds */
        m_soundsStarted = true;
        m_lastSoundPosition = position;
        m_wasOnGround = onGround;
        return;
    }
    m_lastSoundPosition = position;

    WaterRenderer *water = RenderingMaster::getInstance()->water;
    float feet = onGround ? groundY : position.y - RIDE_HEIGHT;
    bool inWater = water != nullptr && water->covers(position.x, position.z) && feet < water->getHeight() - 0.5f;

    if (!onGround) {
        m_fallSpeed = std::max(m_fallSpeed, -moved.y / TICK);
    }
    if (inWater && !m_wasInWater) {
        float strength = glm::clamp((m_fallSpeed + perSecond * 0.5f) / 250.0f, 0.25f, 1.0f);
        audio->play("splash", strength, 0.95f + 0.1f * strength);
    } else if (onGround && !m_wasOnGround && m_fallSpeed > 60.0f) {
        float strength = glm::clamp(m_fallSpeed / 250.0f, 0.5f, 1.0f);
        if (inWater) audio->playVariant("wade", 3, strength);
        else {
            /* a heavy landing: both feet at once, lower and louder (the map's
             * own step sound: sand, grass...) */
            audio->playVariant(steps.sound, steps.variants, steps.volume * (0.8f + 0.4f * strength), 0.04f, 0.8f);
            audio->playVariant(steps.sound, steps.variants, steps.volume * strength, 0.04f, 0.88f);
            if (steps.dust != nullptr) {
                steps.dust->burst(glm::vec3(position.x, feet + 1.0f, position.z), (int) (steps.dustPerStep * 3 * strength));
            }
        }
        m_strideLeft = 20.0f;   /* the next step a little after landing */
    }
    if (onGround) {
        m_fallSpeed = 0.0f;
        if (perSecond > 20.0f) {
            m_strideLeft -= horizontal;
            if (m_strideLeft <= 0.0f) {
                /* walking ~85 units/s: a step every ~40; running, longer strides */
                m_strideLeft += glm::mix(40.0f, 58.0f, glm::clamp((perSecond - 85.0f) / 85.0f, 0.0f, 1.0f));
                float loudness = glm::clamp(perSecond / 170.0f, 0.35f, 1.0f);
                if (inWater) audio->playVariant("wade", 3, 0.5f * loudness);
                else {
                    audio->playVariant(steps.sound, steps.variants, steps.volume * loudness);
                    if (steps.dust != nullptr) {
                        /* kicked up where the foot lands, ahead (so it is seen in
                         * first person, settling as you walk through it) */
                        glm::vec2 heading = glm::normalize(glm::vec2(moved.x, moved.z));
                        float ahead = 18.0f + 0.1f * perSecond;
                        glm::vec3 at(position.x + heading.x * ahead, feet + 1.0f, position.z + heading.y * ahead);
                        steps.dust->burst(at, std::max(1, (int) (steps.dustPerStep * (0.5f + loudness))));
                    }
                }
            }
        } else {
            m_strideLeft = std::min(m_strideLeft, 12.0f);   /* starting off: a step soon */
        }
    }
    m_wasOnGround = onGround;
    m_wasInWater = inWater;
}

void Player::PlayerControllerComponent::render() {

}

const unsigned int Player::PlayerControllerComponent::getFlag() const {
    return Entity::Flags::PLAYER_CONTROLLER;
}

void Player::PlayerControllerComponent::jump(PhysicsComponent *physicsComponent) {
    btRigidBody *body = physicsComponent->getRigidBody();
    btVector3 velocity = body->getLinearVelocity();
    m_isJumping = true;
    body->activate(true);
    body->setLinearVelocity(btVector3(velocity.x(), JUMP_VELOCITY, velocity.z()));
}

Player::~Player () {
    //dtor
}
