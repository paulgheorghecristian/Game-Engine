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

#include <vector>
#include <chrono>

class EngineCore
{
    public:
        EngineCore(rapidjson::Document &gameDocument);
        void input();
        void render();
        void update();
        void start();
        void stop();
        std::vector<Entity *> &getEntities();
        virtual ~EngineCore();

        std::string dumpJson();

    protected:

    private:
        Input inputManager;
        std::vector<Entity *> entities;
        bool isRunning;

        float mouseMoveSpeed, mouseRotationSpeed;
        bool warpMouse, showMouse;
        int outputType;

        void constructPlayer();
        void constructEnemy();
        void loadLights(rapidjson::Document &gameDocument);
        void loadEntities(rapidjson::Document &gameDocument);

        BarGUI *fpsGUI;

        std::string m_configFilePath;
        float m_gravity;

        float secondFraction;

        Player *player;
        Entity *enemy;
        std::unordered_map<std::size_t, Entity *> quadTreeEntities;
        std::vector<std::size_t> lastFoundPathToPlayer;

        void findPathToPlayer(std::size_t source);
};

#endif // ENGINECORE_H
