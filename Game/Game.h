#ifndef GAME_H
#define GAME_H

#include "EngineCore.h"

class Game
{
    public:
        Game(const std::string &gameJsonFilePath);
        void start();
        virtual ~Game();

    protected:

    private:
        EngineCore *engineCore;
};

#endif // GAME_H
