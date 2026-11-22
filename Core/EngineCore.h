#ifndef ENGINECORE_H
#define ENGINECORE_H

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
        EngineCore(rapidjson::Document &gameDocument);
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

    protected:

    private:
        Input inputManager;
        std::vector<Entity *> entities;
        bool isRunning;
        bool isOnStartMenu;

        float mouseMoveSpeed, mouseRotationSpeed;
        bool warpMouse, showMouse;
        int outputType;

        void constructPlayer();
        void constructEnemy();
        void loadLights(rapidjson::Document &gameDocument);
        void loadEntities(rapidjson::Document &gameDocument);
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
