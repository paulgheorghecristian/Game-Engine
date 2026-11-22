#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>
#include "Display.h"
#include <glm/glm.hpp>
#include <vector>
#include <utility>

class Input
{
    public:
        Input();
        /* clearEdges() + pollEvents(): one call per tick (menu, benchmark) */
        void update(Display *);
        /* Rendering runs faster than the game tick: events are polled every
         * frame, and the "pressed/released this tick" flags are kept until a
         * tick has consumed them (clearEdges()). */
        void pollEvents(Display *);
        void clearEdges();
        /* mouse movement since the last call (warped mouse look), then reset */
        glm::vec2 consumeMouseDelta();
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

        /* Scripted input (benchmark scenarios): real mouse motion/warping is
         * ignored and injected key/mouse state is applied after each update(). */
        void setScripted(bool scripted);
        void injectKey(int key, bool held);
        void injectMouseDelta(const glm::vec2 &d);
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
        bool m_scripted = false;
        glm::vec2 m_injectedDelta = glm::vec2(0);
        std::vector<std::pair<int, bool>> m_injectedKeys;
        void applyKey(int key, bool held);
};

#endif // INPUT_H
