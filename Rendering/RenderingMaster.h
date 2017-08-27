#ifndef RENDERINGMASTER_H
#define RENDERINGMASTER_H

#include "rapidjson/document.h"
#include "Display.h"
#include "Camera.h"
#include "Entity.h"
#include "Shader.h"
#include "RenderComponent.h"

#include <vector>
#include <algorithm>

class RenderingMaster
{
    public:
        RenderingMaster(Display *display,
                         Camera *camera,
                         glm::mat4 projectionMatrix,
                         std::vector<Entity *> &entities);
        void update();
        void render();
        void moveCameraForward(float distance);
        void moveCameraSideways(float distance);
        void rotateCameraX(float rotX);
        void rotateCameraY(float rotY);
        Display *getDisplay();
        virtual ~RenderingMaster();

    protected:

    private:
        Display * display;
        Camera * camera;
        glm::mat4 projectionMatrix;
        std::vector<Entity *> &entities;
        std::vector<Shader *> shaders;

        void clearScreen(float r, float g, float b, float a);
        void swapBuffers();
};

#endif // RENDERINGMASTER_H
