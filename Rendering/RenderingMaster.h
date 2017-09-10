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
        static void init (Display *display,
                          Camera *camera,
                          glm::mat4 projectionMatrix);
        static RenderingMaster *getInstance();
        static void destroy ();
        static void clearScreen(float r, float g, float b, float a);
        static void swapBuffers();
        static void moveCameraForward(float distance);
        static void moveCameraSideways(float distance);
        static void rotateCameraX(float rotX);
        static void rotateCameraY(float rotY);
        static Display *getDisplay();
        static Camera *getCamera();
        static glm::mat4 &getProjectionMatrix();
    protected:

    private:
        RenderingMaster(Display *display,
                         Camera *camera,
                         glm::mat4 projectionMatrix);
        ~RenderingMaster();
        static RenderingMaster *m_instance;

        static Display *display;
        static Camera *camera;
        static glm::mat4 projectionMatrix;
};

#endif // RENDERINGMASTER_H
