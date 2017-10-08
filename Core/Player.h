#ifndef PLAYER_H
#define PLAYER_H

#include "Entity.h"
#include "Camera.h"
#include "PhysicsComponent.h"

#define NECK_HEIGHT 10
#define PLAYER_SPEED 450

class Player : public Entity
{
    public:
        Player(Camera *camera, Transform &transform);
        virtual ~Player();

    protected:

    private:
        class FirstPersonComponent : public Component
        {
            public:
                FirstPersonComponent (Camera *camera);

                void input(Input &inputManager);
                void update();
                void render();
                const unsigned int getFlag() const;
                void init();
            private:
                Camera *camera;
        };
};

#endif // PLAYER_H
