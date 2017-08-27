#include "input.h"

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
    for(unsigned int i = 0; i < NUM_MOUSE; i++){
        downMouse[i] = false;
        upMouse[i] = false;
    }

    while(SDL_PollEvent(&event)){
        switch(event.type){
            case SDL_QUIT:{
                Display::isWindowClosed = true;
                break;
            }
            case SDL_KEYDOWN:{
                if(event.key.keysym.sym < 1024 && event.key.keysym.sym >= 0){
                    downKeys[event.key.keysym.sym] = true;
                    inputs[event.key.keysym.sym] = true;
                }else if(event.key.keysym.sym >= 1073741903){
                    int key = event.key.keysym.sym - 1073741903;
                    downKeys[key] = true;
                    inputs[key] = true;
                }

                break;
            }
            case SDL_KEYUP:{
                if(event.key.keysym.sym < 1024 && event.key.keysym.sym >= 0){
                    upKeys[event.key.keysym.sym] = true;
                    inputs[event.key.keysym.sym] = false;
                }else if(event.key.keysym.sym >= 1073741903){
                    int key = event.key.keysym.sym - 1073741903;
                    upKeys[key] = true;
                    inputs[key] = false;
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
    return downKeys[key];
}
bool Input::getKeyUp(int key){
    return upKeys[key];
}
bool Input::getKey(int key){
    return inputs[key];
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

