#ifndef BILLBOARDCOMPONENT_H
#define BILLBOARDCOMPONENT_H

#include "Component.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Camera.h"


class BillboardComponent : public Component
{
    public:
        BillboardComponent();
        void input(Input &inputManager);
        void update();
        void render();
        void init();
        const unsigned int getFlag() const;
        virtual ~BillboardComponent();

    protected:

    private:
        Camera *camera;
};

#endif // BILLBOARDCOMPONENT_H
