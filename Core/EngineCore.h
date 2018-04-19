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

    protected:

    private:
        Input inputManager;
        std::vector<Entity *> entities;
        bool isRunning;

        float mouseMoveSpeed, mouseRotationSpeed;
        bool warpMouse, showMouse;
        int outputType;

        void constructPlayer();
};

#endif // ENGINECORE_H
