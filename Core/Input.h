#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>
#include "Display.h"
#include <glm/glm.hpp>

class Input
{
    public:
        Input();
        void update(Display *);
        bool getKeyDown(int);
        bool getKeyUp(int);
        bool getKey(int);
        bool getMouseDown(int);
        bool getMouseUp(int);
        bool getMouse(int);
        const glm::vec2 &getMousePos();
        const glm::vec2 &getCorrectedMousePos();
        const glm::vec2 &getMouseDelta();
        const glm::vec2 &getMouseDelta2();
        void setWarpMouse(bool warpMouse);
        bool getWarpMouse();
        virtual ~Input();
    protected:
    private:
        static const int NUM_KEYS = 127 + 1;
        static const int NUM_SPECIAL_KEYS = 1073742106-1073741881 + 1;
        static const int NUM_MOUSE = 10;
        bool inputs[NUM_KEYS];
        bool downKeys[NUM_KEYS];
        bool upKeys[NUM_KEYS];
        bool specialInputs[NUM_SPECIAL_KEYS];
        bool specialDownKeys[NUM_SPECIAL_KEYS];
        bool specialUpKeys[NUM_SPECIAL_KEYS];
        bool inputMouse[NUM_MOUSE];
        bool downMouse[NUM_MOUSE];
        bool upMouse[NUM_MOUSE];
        glm::vec2 mousePosition, lastMousePosition, delta, delta2;
        glm::vec2 correctedMousePosition;
        volatile bool warpMouse;
};

#endif // INPUT_H
