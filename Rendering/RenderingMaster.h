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
#include "SmokeParticle.h"

#include <vector>

template<typename P>
class ParticleRenderer;

enum GUIVarsEnum_f {
    VAR1_f = 0,
    VAR2_f,
    VAR3_f,
    VAR4_f,
    VAR5_f,
    VAR6_f,
    NUM_VARS_f
};

enum GUIVarsEnum_int {
    VAR1_i = 0,
    NUM_VARS_i
};

enum GUIVarsEnum_vec3 {
    VAR1_v3 = 0,
    VAR2_v3,
    NUM_VARS_v3
};

class RenderingMaster
{
    public:
        static void init (Display *display,
                          Camera *camera,
                          glm::mat4 projectionMatrix);
        static RenderingMaster *getInstance();
        static void destroy ();
        static Shader simpleTextShader;

        void clearScreen(float r, float g, float b, float a, GLbitfield field);
        void swapBuffers();
        void update();
        void moveCameraForward(float distance);
        void moveCameraSideways(float distance);
        void rotateCameraX(float rotX);
        void rotateCameraY(float rotY);
        Display *getDisplay();
        Camera *getCamera();
        const glm::mat4 &getProjectionMatrix();
        GBuffer &getGBuffer();

        Shader deferredShading_SceneShader;
        Shader deferredShading_BufferCombinationShader;
        Shader deferredShading_StencilBufferCreator;
        Shader depthMapCreator;

        void drawDeferredShadingBuffers();
        void createLightAccumulationBuffer();

        void addLightToScene (Light *light);
        void resetLights ();
        void beginCreateDepthTextureForLight(Light *light);
        void endCreateDepthTextureForLight(Light *light);

        void drawSky();
        void renderVolumetricLight();

        void renderIMGUI();
        void startIMGUIFrame();
        void imguiDrawCalls();
        void computeWorldPosRay(float mouseX, float mouseY);
        const glm::vec3 &getCurrWorldPosRay();

        static glm::vec3 sunLightColor, sunLightDirection;

        const std::vector <Light *> &getLights();
        const glm::vec3 &getWorldPosRay();
        /* TODO make these private */
        ParticleRenderer<SmokeParticle> *smokeRenderer;
        FrameBuffer particleForwardRenderFramebuffer;
        Texture particlesRTTexture;
        FrameBuffer spotLightFlares;
        Texture spotLightFlaresTxt;
        Shader flareShader;
        Texture *depthTexture;

        Entity skyDomeEntity;
        Shader *skyShader;

        Shader volumetricLightShader;

    private:
        RenderingMaster(Display *display,
                         Camera *camera,
                         glm::mat4 projectionMatrix);
        ~RenderingMaster();
        static RenderingMaster *m_instance;

        Display *display;
        Camera *camera;
        glm::mat4 projectionMatrix;
        GBuffer gBuffer;

        Texture *albedoTexture, *normalTexture, *lightAccumulationTexture;\
        Texture *roughnessTexture;
        //Texture *blurredLightAccTexture;
        Mesh *screenSizeRectangle;

        void computeStencilBufferForLight(Light *light);
        void computeLightAccumulationBufferForLight(Light *light);

        void updateLastSpotLight();
        glm::vec3 getWorldSpaceMouseRay(float mouseX, float mouseY);

        PostProcess *hBlurPostProcess,
                    *wBlurPostProcess;
        PostProcess *flarePostProcess;

        Texture *lensFlareColorTxt;
        double updateDt;

        std::vector <Light *> lights;

        FrameBuffer volumetricLightFB;
        Texture volumetricLightTxt;

        float data_f[GUIVarsEnum_f::NUM_VARS_f];
        int data_i[GUIVarsEnum_int::NUM_VARS_i];
        glm::vec3 data_vec3[GUIVarsEnum_vec3::NUM_VARS_v3];

        glm::vec3 currMouseRayWorldPos;
        Light *currentLight;
};

#endif // RENDERINGMASTER_H
