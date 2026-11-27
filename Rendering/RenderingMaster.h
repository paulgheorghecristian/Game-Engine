#ifndef RENDERINGMASTER_H
#define RENDERINGMASTER_H

#include <memory>
#include "MoonDestruction.h"
#include "Waking.h"
#include "BounceLight.h"
#include "RoomAlive.h"
#include "BlackHoleGlow.h"

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
        /* a light taken out of the scene and deleted: not at once but when
         * the frame is over (deleteRemovedLights), as the frame under way may
         * still hold it (its interpolated transform, to put back) */
        void removeLightFromScene(Light *light);
        void deleteRemovedLights();

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
        /* the land around the camera, for the lens of a black hole in the world near the
         * screen's edge (drawn only while one is on screen; after the scene, before lighting) */
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
        std::unique_ptr<Shader> skyShader;

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
        std::unique_ptr<Texture> blueNoiseTexture;
        BounceLight m_bounceLight;      /* light bouncing between near surfaces (Environment "bounceLight") */
        std::unique_ptr<Texture> m_moonTexture;   /* sky: lunar map (equirectangular), unit 15 */
        Mesh *screenSizeRectangle;

        void computeStencilBufferForLight(Light *light);
        void computeLightAccumulationBufferForLight(Light *light);

        void updateLastSpotLight();

        PostProcess *hBlurPostProcess,
                    *wBlurPostProcess;
        PostProcess *flarePostProcess;
        PostProcess *fxaaPostProcess;

        std::unique_ptr<Texture> lensFlareColorTxt;
        double updateDt;

        std::vector<Light *> lights;
        std::vector<Light *> m_removedLights;

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
        /* which hole's sky it holds, and from which frame: it depends only on the hole's place in
         * the sky, so the water's mirror image and the view itself use the same one */
        const BlackHole *m_lensSkyHole = nullptr;
        int m_lensSkyFrame = -1, m_frameNumber = 0;
        static const int LENS_SKY_SIZE = 1024;
        void renderLensSky(const BlackHole &hole);
        void renderBlackHoles(FrameBuffer &target);
        FrameBuffer *m_mirrorLensCopyFB = nullptr;   /* the same copy for the water's reflection */
        GLuint m_farDepthTexture = 0;                /* 1x1, depth 1: "all sky" */
    public:
        /* the black holes into the water's reflection: over its sky, before
         * the mirrored scene is drawn on top (target: the bound reflection) */
        void renderBlackHolesMirrored(FrameBuffer &target, const glm::mat4 &mirroredView, const glm::vec3 &mirroredEye);
    private:
        bool m_skyCloudCut = false;   /* a sky black hole with clouds: split the cloud layer */
        std::string m_skyCubeKey;           /* the environment the sky cube was last drawn with */
        MoonDestruction m_moonDestruction;   /* the Moon's destruction (a Kugelblitz in the sky's hole) */
        Waking m_waking;                     /* a map's waking-up scene */
        RoomAlive m_roomAlive;               /* a map's room coming alive */
        BlackHoleGlow m_blackHoleGlow;       /* the glow of the black holes' own light */
    public:
        /* the Kugelblitz aftermath's weather (Core/Aftermath), each on its own, about 0..1:
         * wind blowing, fog thickening, clouds covering and racing, waves swelling;
         * calm 0..1 stills the map's own wind */
        void setWeather(float wind, float fog, float clouds, float waves, float calm) {
            m_wind = wind; m_fog = fog; m_clouds = clouds; m_waves = waves; m_calm = calm;
        }
        /* everything that moves by itself stopped (time, particles): the timeline editor's pause */
        void setWorldPaused(bool paused) { m_worldPaused = paused; }
        bool isWorldPaused() const { return m_worldPaused; }
        /* what the storm has set by now, as JSON members (the timeline editor's state line) */
        std::string stormState();
        float getWaves() const { return m_waves; }
        /* the map's own wind (its time of day), without the aftermath's wind or calm */
        float getMapWind() const { return m_mapWind; }
        /* the storm's wind direction (xz) and how far the wind has turned to it (0..1) */
        void setStormDirection(const glm::vec2 &direction, float turn) { m_stormDirection = direction; m_stormTurn = turn; }
        /* 0: normal, 1: the picture is black (the ending) */
        void setFadeOut(float fade) { m_fadeOut = fade; }
        /* the storm's dust: dust 0..1 thickens the fog into it; veil 0..1
         * covers the picture (1: nothing but dust) */
        void setDust(float dust, float veil) { m_dust = dust; m_dustVeil = veil; }
        /* the storm's water: rain 0..1 streaking past, lens 0..1 drops on the lens */
        void setStormWater(float rain, float lens) { m_rain = rain; m_lensWater = lens; }
        /* a Kugelblitz's debris in the sky (its timeline's "debrisShell": 0 at the hole, 1 reached
         * us, 2 gone) and the matter it throws straight at us ("ejecta", 0..1). < 0: not set, they
         * follow the Moon (the shell reaches us BlackHole::DEBRIS_ARRIVAL seconds after it formed) */
        void setDebris(float shell, float ejecta) { m_debrisShell = shell; m_ejecta = ejecta; }
        MoonDestruction &moonDestruction() { return m_moonDestruction; }
        Waking &waking() { return m_waking; }
        RoomAlive &roomAlive() { return m_roomAlive; }
        /* the sky Kugelblitz's playback clock: 1 normal, 0 paused (tools/timeline_editor.py) */
        void setKugelblitzTimeScale(float scale) { m_kugelblitzTimeScale = scale; }
        /* passes skip what the camera cannot see (GE_NO_CULL=1: they draw everything, to compare) */
        static bool viewCulling();
        /* lightning in the dust: flash 0..1 (blinding), seen as a glow inside
         * the dust at screen x boltX, height reach (0..1), shaped by seed */
        void setLightning(float flash, float boltX, float seed, float reach) {
            m_lightning = flash; m_boltX = boltX; m_boltSeed = seed; m_boltReach = reach;
        }
    private:
        float m_lightning = 0.0f, m_boltX = 0.5f, m_boltSeed = 0.0f, m_boltReach = 0.0f;
        float m_dust = 0.0f, m_dustVeil = 0.0f;
        float m_rain = 0.0f, m_lensWater = 0.0f;
        float m_debrisShell = -1.0f, m_ejecta = -1.0f;
        float m_kugelblitzTimeScale = 1.0f;
        bool m_worldPaused = false;
        float m_wind = 0.0f, m_fog = 0.0f, m_clouds = 0.0f, m_waves = 0.0f, m_calm = 0.0f, m_mapWind = 0.0f;
        float m_appliedWeather[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };   /* wind, fog, clouds, calm, dust */
        glm::vec2 m_stormDirection = glm::vec2(1.0f, 0.0f);
        float m_stormTurn = 0.0f;
        float m_fadeOut = 0.0f;
        std::unique_ptr<FrameBuffer> m_prevFrameFB;   /* last frame's HDR image (reflections) */
        std::unique_ptr<FrameBuffer> m_glassCopyFB;   /* the image behind the glass */
        std::unique_ptr<Shader> m_glassShader;
        /* exposure adaptation */
        void updateAdaptation();
        /* (owned: made when first needed, released with this) */
        std::unique_ptr<FrameBuffer> m_luminanceFB, m_adaptationA, m_adaptationB;
        std::unique_ptr<Shader> m_luminanceShader, m_adaptationShader;
        double m_lastAdaptationTime = 0.0;      /* (real time: the eye adapts while the world is paused too) */
        /* temporal anti-aliasing */
        void renderTemporalAA();
        std::unique_ptr<FrameBuffer> m_taaHistory[2];
        std::unique_ptr<Shader> m_taaShader;
        int m_taaIndex = 0, m_taaFrame = 0;
        bool m_taaHistoryValid = false;
        glm::mat4 m_taaPreviousViewProjection = glm::mat4(1.0f);
        void blackHoleGUI();
};

#endif // RENDERINGMASTER_H
