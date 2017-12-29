#include "Display.h"

int Display::delta = 0;
bool Display::isWindowClosed;

Display::Display(const int WIDTH,
                 const int HEIGHT,
                 const char * title,
                 bool fullscreen,
                 unsigned int maxFps,
                 bool vSync = false) : MAX_FPS (maxFps), FRAME_TIME_IN_MS (1000.0f/MAX_FPS) {
    SDL_Init (SDL_INIT_EVERYTHING);

    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 8);

    if (fullscreen) {
        SDL_DisplayMode dm;

        if (SDL_GetDesktopDisplayMode (0, &dm) != 0){
            SDL_Log ("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        }

        this->width = dm.w;
        this->height = dm.h;
    } else {
        this->width = WIDTH;
        this->height = HEIGHT;
    }

    window = SDL_CreateWindow (title,
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               this->width,
                               this->height,
                               SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE));
    glContext = SDL_GL_CreateContext (window);

    GLenum status = glewInit ();
    if (status != GLEW_OK) {
        std::cerr << "Glew failed to initialize !" << std::endl;
    }

    isWindowClosed = false;
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glEnable (GL_CULL_FACE);
    glViewport (0.0f, 0.0f, this->width, this->height);

    if (vSync) {
        SDL_GL_SetSwapInterval  (1);
    }
}

Display::~Display() {
    SDL_GL_DeleteContext (glContext);
    SDL_DestroyWindow (window);
    SDL_Quit ();
}

void Display::clear(float r, float g, float b, float a, GLbitfield field) {
    glClearColor (r, g, b, a);
    glClear (field);
}

void Display::setViewport(float x, float y, float width, float height) {
    glViewport(x, y, width, height);
}

bool Display::isClosed() {
    return isWindowClosed;
}

void Display::swapBuffers() {
    SDL_GL_SwapWindow(window);
}

const int &Display::getWidth() const {
    return width;
}

const int &Display::getHeight() const {
    return height;
}

SDL_Window* Display::getWindow() {
    return window;
}

const unsigned int Display::getMaxFps() const {
    return MAX_FPS;
}

const float Display::getFrameTimeInMs() const {
    return FRAME_TIME_IN_MS;
}

void Display::close() {
    isWindowClosed = true;
}
