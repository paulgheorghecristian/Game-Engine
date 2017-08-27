#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include <iostream>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Display
{
    public:
        Display(const int WIDTH, const int HEIGHT, const char * title, bool fullscreen);
        virtual ~Display();
        void swapBuffers();
        bool isClosed();
        void setViewport(float, float, float, float);
        void clear(float, float, float, float);
		const int getWidth() const;
		const int getHeight() const;
        static bool isWindowClosed;
        static int delta;
        SDL_Window * getWindow();
        void close();
    protected:
    private:
        SDL_Window * window;
        SDL_GLContext glContext;
        float width, height;
};

#endif // DISPLAY_H
