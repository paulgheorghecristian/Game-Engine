#ifndef ENGINECORE_H
#define ENGINECORE_H

#include "FrameRecorder.h"
#include "SoundScape.h"
#include "Timeline.h"
#include "Aftermath.h"
#include "Display.h"
#include "Input.h"
#include "Entity.h"
#include "RenderingMaster.h"
#include "PhysicsMaster.h"
#include "FileUtils.h"
#include "rapidjson/error/en.h"
#include "ComponentFactory.h"
#include "Texture.h"
#include "ProfilingTimer.h"
#include "Player.h"
#include "BarGUI.h"
#include "Menu.hpp"

#include <vector>
#include <chrono>

class EngineCore
{
    public:
        EngineCore(rapidjson::Document &gameDocument, const std::string &mapPath = "");
        void input();
        void render(float alpha = 1.0f);
        void update();
        void start();
        void stop();
        void startMenuLoop();
        void runBenchmark(int numFrames);

        /* Rendering is decoupled from the fixed-rate game tick: each frame
         * draws the world blended between the previous and the current tick. */
        void applyMouseLook();
        void throwFireball();

        /* save the current world (F5) as <map>_saved.json; reload it (F9) */
        std::string m_mapPath, m_savePath;
        std::string m_waterJson, m_emittersJson, m_grassJson, m_audioJson, m_footstepsJson, m_sequencesJson;
        /* the map's "Grass" (needs the terrain entity, so after the entities) */
        void loadGrass(rapidjson::Document &gameDocument);
        bool saveMap(const std::string &path);
        void reloadMap(const std::string &path);
        std::vector<std::pair<Entity *, float>> m_fireballs; /* entity, spawn time */
        void capturePreviousState();
        void beginInterpolation(float alpha);
        void endInterpolation();
        struct {
            bool valid = false;
            glm::vec3 cameraPosition;
            std::unordered_map<Entity *, Transform> entities;
            std::unordered_map<Light *, Transform> lights;
        } m_previousTick;
        glm::vec3 m_tickCameraPosition;
        std::vector<std::pair<Entity *, Transform>> m_stashedEntities;
        std::vector<std::pair<Light *, Transform>> m_stashedLights;
        unsigned int m_maxRenderFps = 0; /* 0: unlimited (vsync still applies) */
        std::vector<Entity *> &getEntities();
        virtual ~EngineCore();

        std::string dumpJson();
        std::string mapJson();

    protected:

    private:
        Input inputManager;
        std::vector<Entity *> entities;
        bool isRunning;
        bool isOnStartMenu;

        float mouseMoveSpeed, mouseRotationSpeed;
        bool warpMouse, showMouse;
        /* play mode (the default once the game starts): no overlays (profiler,
         * FPS, ImGui) and no cursor; F1 toggles it, to open the editor GUI */
        bool m_playMode = false;
        bool m_warpBeforePlayMode = false;
        void setPlayMode(bool play);
        /* F12: record what is rendered to ~/Videos (MP4, without overlays) */
        FrameRecorder m_recorder;
        /* the map's sound ("Audio" block) and the Kugelblitz's */
        SoundScape m_soundScape;
        Aftermath m_aftermath;     /* the storm after the Kugelblitz (driven by the timeline) */
        Timeline m_timeline;       /* the map's scripted sequences ("Sequences") */
        int outputType;

        void constructPlayer();
        void constructEnemy();
        void loadLights(rapidjson::Document &gameDocument);
        void loadEntities(rapidjson::Document &gameDocument);
        RenderingObject makeSpawnedBox();
        void spawnBox(const glm::vec3 &position);
        /* optional map sections: Environment, Water, ParticleEmitters, PlayerStart */
        void loadMapFeatures(rapidjson::Document &gameDocument);
        glm::vec3 m_playerStart = glm::vec3(200, 10, 0);
        float m_playerYaw = 0.0f, m_playerPitch = 0.0f; /* degrees */

        BarGUI *fpsGUI;

        std::string m_configFilePath;
        float m_gravity;

        float secondFraction;

        Player *player;
        Entity *enemy;
        std::unordered_map<std::size_t, Entity *> m_quadTreeEntities;
        std::vector<std::size_t> lastFoundPathToPlayer;

        void findPathToPlayer();

        Menu *startMenu;
        Menu *optionsMenu;
        Menu *currentMenu;

        /* GE_BENCH=N benchmark mode: no menu, windowed, no vsync, fixed timestep */
        int m_benchFrames = 0;
        std::string m_screenshotPath;
        void saveScreenshot(const std::string &path);
        std::vector<unsigned char> *m_captureBeforeSwap = NULL;
};

#endif // ENGINECORE_H
