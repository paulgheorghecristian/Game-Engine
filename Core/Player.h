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
        };

        class PlayerControllerComponent : public Component
        {
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
        };
};

#endif // PLAYER_H
