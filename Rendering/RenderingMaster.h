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

        static glm::vec3 sunLightColor, sunLightDirection;

        const std::vector <Light *> &getLights();
        /* TODO make these private */
        ParticleRenderer<SmokeParticle> *smokeRenderer;
        FrameBuffer particleForwardRenderFramebuffer;
        Texture particlesRTTexture;
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

        Texture *albedoTexture, *normalTexture, *lightAccumulationTexture;
        Texture *blurredLightAccTexture;
        Mesh *screenSizeRectangle;

        void computeStencilBufferForLight(Light *light);
        void computeLightAccumulationBufferForLight(Light *light);

        void updateLastSpotLight();

#if 0
        PostProcess *hBlurPostProcess,
                    *wBlurPostProcess,
                    *brightnessControlPostProcess;
#endif
        double updateDt;

        std::vector <Light *> lights;

        FrameBuffer volumetricLightFB;
        Texture volumetricLightTxt;
};

#endif // RENDERINGMASTER_H
