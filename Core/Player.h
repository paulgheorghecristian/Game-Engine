#ifndef PLAYER_H
#define PLAYER_H

#include "Entity.h"
#include "Camera.h"
#include "PhysicsComponent.h"

class ParticleEmitter;

class Player : public Entity
{
    public:
        Player(Transform &transform);
        virtual ~Player();

        std::string jsonify();

        /* the map's footsteps ("Footsteps" in the map JSON, all optional):
         *   "sound": the step sounds name_1..name_<variants> (step_grass),
         *   "variants": 6, "volume": 1.0,
         *   "dust": a "burst" particle emitter kicked up at each step
         *           (and more on landing), "dustPerStep": 6 */
        struct Footsteps {
            std::string sound = "step_grass";
            int variants = 6;
            float volume = 1.0f;
            ParticleEmitter *dust = nullptr;   /* owned by RenderingMaster's emitter list */
            int dustPerStep = 6;
        };
        static Footsteps &footsteps();

    protected:

    private:
        class FirstPersonComponent : public Component
        {
            public:
                FirstPersonComponent ();

                void input(Input &inputManager);
                void update();
                void render();
                const unsigned int getFlag() const;
                void init();
            private:
                Camera *m_camera;
                glm::vec3 lastPos, lastPosDiff;
                float sineStep;
                bool dir;
                bool animFOV;
                float currFOV;
        };

        class PlayerControllerComponent : public Component
        {
            friend class FirstPersonComponent;
            public:
                PlayerControllerComponent ();

                void input(Input &inputManager);
                void update();
                void render();
                const unsigned int getFlag() const;
            private:
                void jump(PhysicsComponent *physicsComponent);

                bool m_isJumping, m_wasSpaceReleased;
                /* forgiving jumps: still allowed shortly after leaving the
                 * ground (coyote time), and a press shortly before landing
                 * is kept until it can be used (jump buffer); seconds */
                float m_groundTimer = 0.0f, m_jumpBuffer = 0.0f;
                /* sound: footsteps by distance walked, landing, water */
                void playSounds(bool onGround, float groundY, const glm::vec3 &position);
                float m_strideLeft = 0.0f;
                bool m_wasOnGround = true, m_wasInWater = false, m_soundsStarted = false;
                float m_fallSpeed = 0.0f;
                glm::vec3 m_lastSoundPosition = glm::vec3(0.0f);
                Camera *m_camera;
                bool m_freeRoam;
                float speed;
        };
};

#endif // PLAYER_H
