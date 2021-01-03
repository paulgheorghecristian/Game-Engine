#include "Input.h"

#include "imgui_impl_sdl.h"

Input::Input() : mousePosition(0.0)
{
    for(unsigned int i = 0; i < NUM_KEYS; i++){
        downKeys[i] = false;
        upKeys[i] = false;
        inputs[i] = false;
    }
    for(unsigned int i = 0; i < NUM_MOUSE; i++){
        downMouse[i] = false;
        upMouse[i] = false;
        inputMouse[i] = false;
    }

    warpMouse = true;
}

Input::~Input()
{
    //dtor
}

void Input::update(Display * display){
    SDL_Event event;

    for(unsigned int i = 0; i < NUM_KEYS; i++){
        downKeys[i] = false;
        upKeys[i] = false;
    }
    for(unsigned int i = 0; i < NUM_SPECIAL_KEYS; i++){
        specialDownKeys[i] = false;
        specialUpKeys[i] = false;
    }
    for(unsigned int i = 0; i < NUM_MOUSE; i++){
        downMouse[i] = false;
        upMouse[i] = false;
    }

    while (SDL_PollEvent(&event)){
        switch (event.type){
            case SDL_QUIT:{
                Display::isWindowClosed = true;
                break;
            }
            case SDL_KEYDOWN:{
                if (event.key.keysym.sym <= 127 && event.key.keysym.sym >= 0) {
                    downKeys[event.key.keysym.sym] = true;
                    inputs[event.key.keysym.sym] = true;
                } else if (event.key.keysym.sym >= 1073741881) {
                    int key = event.key.keysym.sym - 1073741881;
                    specialDownKeys[key] = true;
                    specialInputs[key] = true;
                }

                break;
            }
            case SDL_KEYUP:{
                if (event.key.keysym.sym <= 127 && event.key.keysym.sym >= 0) {
                    upKeys[event.key.keysym.sym] = true;
                    inputs[event.key.keysym.sym] = false;
                } else if(event.key.keysym.sym >= 1073741881) {
                    int key = event.key.keysym.sym - 1073741881;
                    specialUpKeys[key] = true;
                    specialInputs[key] = false;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:{
                downMouse[event.button.button] = true;
                inputMouse[event.button.button] = true;
                break;
            }
            case SDL_MOUSEBUTTONUP:{
                upMouse[event.button.button] = true;
                inputMouse[event.button.button] = false;
                break;
            }
            case SDL_MOUSEMOTION:{
                mousePosition = glm::vec2(event.motion.x, event.motion.y);
                delta = glm::vec2(display->getWidth()/2.0f, display->getHeight()/2.0f) - mousePosition;
                if (!warpMouse) {
                    delta = glm::vec2(0,0);
                }
                break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
    if (warpMouse) {
        SDL_WarpMouseInWindow(display->getWindow(), display->getWidth()/2, display->getHeight()/2);
    }
}

void Input::setWarpMouse(bool warpMouse) {
    this->warpMouse = warpMouse;
}

bool Input::getWarpMouse() {
    return warpMouse;
}

bool Input::getKeyDown(int key){
    if (key <= 127)
        return downKeys[key];
    else
        return specialDownKeys[key-1073741881];
}
bool Input::getKeyUp(int key){
    if (key <= 127)
        return upKeys[key];
    else
        return specialUpKeys[key-1073741881];
}
bool Input::getKey(int key){
    if (key <= 127)
        return inputs[key];
    else
        return specialInputs[key-1073741881];
}
bool Input::getMouseDown(int key){
    return downMouse[key];
}
bool Input::getMouseUp(int key){
    return upMouse[key];
}
bool Input::getMouse(int key){
    return inputMouse[key];
}
glm::vec2 Input::getMousePos(){
    return mousePosition;
}

const glm::vec2 &Input::getMouseDelta(){
    return delta;
}

