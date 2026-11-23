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
#include "DebugRenderer.hpp"
#include "Environment.h"

#include <vector>
#include <functional>
#include <string>
#include <algorithm>

template<typename P>
class ParticleRenderer;
class ParticleEmitter;
class WaterRenderer;
class GrassRenderer;
class BlackHole;

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
                          glm::mat4 projectionMatrix,
                          float renderScale = 1.0f);
        static RenderingMaster *getInstance();
        static void destroy ();
        static Shader simpleTextShader;

        void clearScreen(float r, float g, float b, float a, GLbitfield field);
        void swapBuffers();
        /* game tick: simulation time, particles */
        void update();
        /* every rendered frame: camera-dependent uniforms, shadow matrices and
         * the render time; alpha = how far the frame is between the previous
         * and the current tick (0..1) */
        void prepareFrame(float alpha);
        void moveCameraForward(float distance);
        void moveCameraSideways(float distance);
        void rotateCameraX(float rotX);
        void rotateCameraY(float rotY);
        Display *getDisplay();
        Camera *getCamera();
        const glm::mat4 &getProjectionMatrix();
        inline const glm::mat4 &getInverseViewMatrix() { return inverseViewMatrix; }
        GBuffer &getGBuffer();

        /* The 3D scene is rendered at renderScale * window size and upscaled
         * by the final (FXAA) pass; GUI/ImGui are drawn at window resolution. */
        inline int getRenderWidth() const { return renderWidth; }
        inline int getRenderHeight() const { return renderHeight; }

        void setFOV(float fov);
        /* moves the camera's far plane (keeps fov / aspect / near) */
        void setFarPlane(float farPlane);
        float getFarPlane() const { return m_farPlane; }
        /* a bounding sphere entirely past the draw distance (fully fogged) */
        bool isBeyondDrawDistance(const glm::vec3 &center, float radius) const;

        Shader deferredShading_SceneShader;
        Shader deferredShading_InstanceRender;
        Shader deferredShading_AnimRender;
        Shader deferredShading_BufferCombinationShader;
        Shader deferredShading_StencilBufferCreator;
        Shader depthMapCreator;
        Shader depthMapCreatorInstanceRender;
        Shader lastStageShader;

        void drawDeferredShadingBuffers();
        void createLightAccumulationBuffer();

        void addLightToScene (Light *light);
        void resetLights ();
        void beginCreateDepthTextureForLight(Light *light);
        void endCreateDepthTextureForLight(Light *light);

        void drawSky();
        /* the sky of the main view (leaves the clouds around a sky black hole
         * out; they are drawn over the hole afterwards) */
        void drawMainSky();
        void renderVolumetricLight();

        void renderIMGUI();
        void startIMGUIFrame();
        void imguiDrawCalls();

        std::vector<Light *> &getLights();

        /* per-map atmosphere (sky, fog, ambient, emissive); defaults = original look */
        void setEnvironment(const Environment &environment);
        /* day-night: speed multiplier for the cycle (T key cycles 1x/10x/60x) */
        void cycleTimeScale();
        void setTimeOfDay(float timeOfDay);
        /* 1 at night, 0 by day ("nightOnly" lights fade with it) */
        float getNightFactor() const;
        inline const Environment &getEnvironment() const { return m_environment; }
        /* the map's configuration (+ GUI edits) at the current time of day, for saving */
        Environment getEnvironmentToSave() const {
            Environment e = m_baseEnvironment;
            e.timeOfDay = m_environment.timeOfDay;
            e.timePaused = m_timePaused;
            return e;
        }
        /* directional light as configured (the day-night cycle overrides it at runtime) */
        bool getDirectionalLightBase(glm::vec3 &color, glm::vec3 &direction) const {
            color = m_dirLightBaseColor;
            direction = m_baseEnvironment.moonDirection;
            return m_baseEnvironment.dayLength > 0.0f && m_dirLightBaseCaptured;
        }
        /* map save / reload, provided by the engine (GUI buttons, F5 / F9) */
        std::function<void()> onSaveMap, onReloadMap;
        std::string savedMapPath;
        /* seconds of simulated time, interpolated for the frame being rendered */
        inline float getTime() const { return m_renderTime; }
        /* fog cut-off uniforms for passes composited after the fog (particles, beams) */
        void uploadFogCutoff(Shader &shader) const;
        /* wind uniforms (direction, strength, time) for shaders that sway geometry */
        void uploadWind(Shader &shader) const;
        void uploadGlowLights(Shader &shader);
        void uploadHeat(Shader &shader);
        glm::vec2 getWindDirection() const;
        inline float getRenderAlpha() const { return m_renderAlpha; }

        Texture &getVolumetricLightTexture() { return volumetricLightTxt; }
        Texture &getFlareTexture();

        /* optional per-map features; owned by RenderingMaster */
        WaterRenderer *water = NULL;
        GrassRenderer *grass = NULL;
        std::vector<BlackHole *> blackHoles;
        /* a full screen quad (post-processing passes) */
        void drawScreenQuad();
        std::vector<ParticleEmitter *> particleEmitters;
        /* glass entities seen this frame (filled while the G-buffer is drawn) */
        std::vector<Entity *> glass;
        void renderGlass(FrameBuffer &target);
        void drawParticles();
        void addParticleEmitter(ParticleEmitter *emitter) { particleEmitters.push_back(emitter); }
        void removeParticleEmitter(ParticleEmitter *emitter) {
            particleEmitters.erase(std::remove(particleEmitters.begin(), particleEmitters.end(), emitter),
                                   particleEmitters.end());
        }

        /* Lens flares only come from billboard entities. With none in the
         * scene the flare buffers are cleared once and their passes skipped. */
        void setFlareSourceCount(int count);
        /* Sun lens flare (day-night maps): the sun disc is drawn into the
         * flare buffer, occluded by the scene, and the flare pass turns it into
         * god rays, ghosts and a halo (ported from World2's sun post-process). */
        bool isSunFlareVisible();
        void drawSunFlareSource();
        inline bool flaresNeeded() const { return m_framesWithoutFlares < 2; }
        const glm::vec3 &getWorldPosRay();
        /* TODO make these private */
        ParticleRenderer<SmokeParticle> *smokeRenderer;
        FrameBuffer particleForwardRenderFramebuffer;
        Texture particlesRTTexture;
        FrameBuffer spotLightFlares;
        Texture spotLightFlaresTxt;
        Shader flareShader;
        Texture *depthTexture;

        FrameBuffer fxaaFrameBuffer;

        Entity skyDomeEntity;
        Shader *skyShader;

        Shader volumetricLightShader;

        float secondFraction;

        static glm::mat4 orthoProjectionMatrix;

    private:
        RenderingMaster(Display *display,
                         Camera *camera,
                         glm::mat4 projectionMatrix,
                         int renderWidth, int renderHeight);
        int renderWidth, renderHeight;
        ~RenderingMaster();
        static RenderingMaster *m_instance;

        Display *display;
        Camera *camera;
        glm::mat4 projectionMatrix;
        glm::mat4 inverseViewMatrix = glm::mat4(1.0f);
        Environment m_environment;      /* current (time of day applied) */
        Environment m_baseEnvironment;  /* as configured by the map */
        float m_timeScale = 1.0f;
        bool m_timePaused = false;
        void environmentGUI();
        bool m_dirLightBaseCaptured = false;
        glm::vec3 m_dirLightBaseColor = glm::vec3(0.0f);
        glm::vec3 m_dirLightBaseDirection = glm::vec3(0.0f, 1.0f, 0.0f);
        void applyEnvironment(const Environment &environment);

        /* post-processing, created on first use */
        FrameBuffer *m_ssaoFB = nullptr, *m_ssaoBlurFB = nullptr;
        FrameBuffer *m_hdrFB = nullptr;
        FrameBuffer *m_bloomHalfA = nullptr, *m_bloomHalfB = nullptr;
        FrameBuffer *m_bloomQuarterA = nullptr, *m_bloomQuarterB = nullptr;
        Shader *m_ssaoShader = nullptr, *m_ssaoBlurShader = nullptr;
        Shader *m_bloomBrightShader = nullptr, *m_blurShader = nullptr, *m_tonemapShader = nullptr;
        void renderSSAO();
        /* PBR sky reflections: the sky rendered into a small cube map (with
         * mipmaps standing in for roughness blur), refreshed every frame */
        GLuint m_skyCube = 0, m_skyCubeFbo = 0;
        static const int SKY_CUBE_SIZE = 128;
        void renderSkyCube();
        void renderBloomAndToneMap();
        void blurPass(FrameBuffer &source, FrameBuffer &target, const glm::vec2 &direction);
        float m_time = 0.0f;
        float m_renderTime = 0.0f;
        float m_renderAlpha = 1.0f;
        int m_framesWithoutFlares = 0;
        Mesh *m_sunDisc = NULL;
        bool sunScreenPosition(glm::vec2 &uv);
        GBuffer gBuffer;

        Texture *albedoTexture, *normalTexture, *lightAccumulationTexture;\
        Texture *roughnessTexture;
        Texture *blueNoiseTexture;
        Texture *m_moonTexture = nullptr;   /* sky: lunar map (equirectangular), unit 15 */
        Mesh *screenSizeRectangle;

        void computeStencilBufferForLight(Light *light);
        void computeLightAccumulationBufferForLight(Light *light);

        void updateLastSpotLight();

        PostProcess *hBlurPostProcess,
                    *wBlurPostProcess;
        PostProcess *flarePostProcess;
        PostProcess *fxaaPostProcess;

        Texture *lensFlareColorTxt;
        double updateDt;

        std::vector<Light *> lights;

        FrameBuffer volumetricLightFB;
        Texture volumetricLightTxt;

        float data_f[GUIVarsEnum_f::NUM_VARS_f];
        int data_i[GUIVarsEnum_int::NUM_VARS_i];
        glm::vec3 data_vec3[GUIVarsEnum_vec3::NUM_VARS_v3];
    private:
        float m_farPlane = 5000.0f, m_defaultFarPlane = 5000.0f;
        void uploadProjection();
        FrameBuffer *m_lensCopyFB = nullptr;   /* black holes read a copy of the image they bend */
        FrameBuffer *m_lensSkyFB = nullptr;    /* sky black holes: the sky behind them, no clouds */
        static const int LENS_SKY_SIZE = 1024;
        void renderLensSky(const BlackHole &hole);
        void renderBlackHoles(FrameBuffer &target);
        bool m_skyCloudCut = false;   /* a sky black hole with clouds: split the cloud layer */
    public:
        /* the Kugelblitz aftermath (Core/Aftermath): storm 0..1 adds wind,
         * fog, clouds and waves; calm 0..1 stills the wind before it hits */
        void setStorm(float storm, float calm) { m_storm = storm; m_calm = calm; }
        float getStorm() const { return m_storm; }
        /* the storm's wind direction (xz) and how far the wind has turned to it (0..1) */
        void setStormDirection(const glm::vec2 &direction, float turn) { m_stormDirection = direction; m_stormTurn = turn; }
        /* 0: normal, 1: the picture is black (the ending) */
        void setFadeOut(float fade) { m_fadeOut = fade; }
    private:
        float m_storm = 0.0f, m_calm = 0.0f;
        float m_appliedStorm = 0.0f, m_appliedCalm = 0.0f;
        glm::vec2 m_stormDirection = glm::vec2(1.0f, 0.0f);
        float m_stormTurn = 0.0f;
        float m_fadeOut = 0.0f;
        FrameBuffer *m_prevFrameFB = nullptr;   /* last frame's HDR image (reflections) */
        FrameBuffer *m_glassCopyFB = nullptr;   /* the image behind the glass */
        Shader *m_glassShader = nullptr;
        /* exposure adaptation */
        void updateAdaptation();
        FrameBuffer *m_luminanceFB = nullptr, *m_adaptationA = nullptr, *m_adaptationB = nullptr;
        Shader *m_luminanceShader = nullptr, *m_adaptationShader = nullptr;
        float m_lastAdaptationTime = 0.0f;
        /* temporal anti-aliasing */
        void renderTemporalAA();
        FrameBuffer *m_taaHistory[2] = {nullptr, nullptr};
        Shader *m_taaShader = nullptr;
        int m_taaIndex = 0, m_taaFrame = 0;
        bool m_taaHistoryValid = false;
        glm::mat4 m_taaPreviousViewProjection = glm::mat4(1.0f);
        float m_lastCollapseTime = -1.0f;
        void blackHoleGUI();
};

#endif // RENDERINGMASTER_H
