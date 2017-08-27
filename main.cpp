#include <iostream>

#include "Game.h"

int main(int argc, char ** argv) {
    Game game ("Game/game.json");
    game.start();
    return 0;
}
