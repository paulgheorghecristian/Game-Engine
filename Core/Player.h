#ifndef PLAYER_H
#define PLAYER_H

#include "Entity.h"
#include "Camera.h"
#include "PhysicsComponent.h"

class Player : public Entity
{
    public:
        Player(Transform &transform);
        virtual ~Player();

        std::string jsonify();

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
                Camera *m_camera;
                bool m_freeRoam;
                float speed;
        };
};

#endif // PLAYER_H
