#ifndef RENDERINGMASTER_H
#define RENDERINGMASTER_H

#include "rapidjson/document.h"
#include "Display.h"
#include "Camera.h"
#include "Entity.h"
#include "Shader.h"
#include "RenderComponent.h"
#include "GBuffer.h"
#include "Texture.h"
#include "Light.h"
#include "PostProcess.h"

#include <vector>

class RenderingMaster
{
    public:
        static void init (Display *display,
                          Camera *camera,
                          glm::mat4 projectionMatrix);
        static RenderingMaster *getInstance();
        static void destroy ();
        static void clearScreen(float r, float g, float b, float a, GLbitfield field);
        static void swapBuffers();
        static void update();
        static void moveCameraForward(float distance);
        static void moveCameraSideways(float distance);
        static void rotateCameraX(float rotX);
        static void rotateCameraY(float rotY);
        static Display *getDisplay();
        static Camera *getCamera();
        static glm::mat4 &getProjectionMatrix();
        static GBuffer &getGBuffer();
        static Shader deferredShading_SceneShader;
        static Shader deferredShading_BufferCombinationShader;
        static Shader simpleTextShader;

        static void drawDeferredShadingBuffers();
        static void createLightAccumulationBuffer();

        static void addLightToScene (Light *light);
        static void beginCreateDepthTextureForSpotLight(Light *light);
        static void endCreateDepthTextureForSpotLight(Light *light);

        static Light *firstSpotLight;
        static Shader deferredShading_StencilBufferCreator;

        static const std::vector <Light *> &getLights();
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
        static GBuffer gBuffer;

        static Texture *albedoTexture, *normalTexture, *lightAccumulationTexture, *depthTexture;
        static Texture *blurredLightAccTexture;
        static Shader deferredShading_LightAccumulationBufferCreator;
        static Mesh *screenSizeRectangle;

        static void computeStencilBufferForLight(Light *light);
        static void computeLightAccumulationBufferForLight(Light *light);

        static PostProcess *hBlurPostProcess,
                           *wBlurPostProcess,
                           *brightnessControlPostProcess;

        static std::vector <Light *> lights;
};

#endif // RENDERINGMASTER_H
