#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "Game.h"

/* On hybrid-graphics laptops Linux renders on the integrated GPU unless asked
 * otherwise. If the NVIDIA driver is present, request PRIME render offload so
 * the engine runs on the discrete GPU (several times faster). Must happen
 * before the GL context is created. GE_GPU=integrated keeps the default GPU. */
static void preferDiscreteGPU() {
    const char *choice = std::getenv("GE_GPU");
    if (choice != NULL && std::strcmp(choice, "integrated") == 0) {
        return;
    }
    if (access("/proc/driver/nvidia/version", F_OK) != 0) {
        return;
    }
    setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
    setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
}

int main(int argc, char ** argv) {
    preferDiscreteGPU();

    /* usage: ./ge [game.json] */
    Game game(argc > 1 ? argv[1] : "Game/lagoon.json");
    game.start();

    return 0;
}
