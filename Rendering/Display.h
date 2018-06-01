#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include <iostream>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Display
{
    public:
        Display(const int WIDTH,
                const int HEIGHT,
                const char * title,
                bool fullscreen,
                unsigned int maxFps,
                bool vSync = false);
        virtual ~Display();
        void swapBuffers();
        bool isClosed();
        void setViewport(float, float, float, float);
        void clear(float, float, float, float, GLbitfield field);
        int getWidth() const;
        int getHeight() const;
        static bool isWindowClosed;
        static int delta;
        SDL_Window * getWindow();
        void close();

        const unsigned int getMaxFps() const;
        const float getFrameTimeInMs() const;
    protected:
    private:
        SDL_Window * window;
        SDL_GLContext glContext;
        int width, height;
        const unsigned int MAX_FPS;
        const float FRAME_TIME_IN_MS;
};

#endif // DISPLAY_H
