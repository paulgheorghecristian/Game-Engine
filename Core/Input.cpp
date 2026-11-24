#include "Input.h"

#include "imgui_impl_sdl.h"

Input::Input() : mousePosition(0.0)
{
    for(unsigned int i = 0; i < NUM_KEYS; i++){
        downKeys[i] = false;
        upKeys[i] = false;
        inputs[i] = false;
    }
    for (unsigned int i = 0; i < NUM_SPECIAL_KEYS; i++) {
        specialDownKeys[i] = false;
        specialUpKeys[i] = false;
        specialInputs[i] = false;
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
    clearEdges();
    pollEvents(display);
}

glm::vec2 Input::consumeMouseDelta() {
    glm::vec2 d = delta;
    delta = glm::vec2(0);
    return d;
}

void Input::clearEdges() {
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
}

void Input::pollEvents(Display *display) {
    SDL_Event event;

    while (SDL_PollEvent(&event)){
        if (m_ignoreUser && (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_MOUSEMOTION ||
                             event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
                             event.type == SDL_MOUSEWHEEL)) {
            continue;
        }
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
                if (m_scripted) break;
                mousePosition = glm::vec2(event.motion.x, event.motion.y);
                correctedMousePosition = glm::vec2(mousePosition.x, display->getHeight() - mousePosition.y);
                delta = glm::vec2(display->getWidth()/2.0f, display->getHeight()/2.0f) - mousePosition;
                delta2 = glm::vec2(event.motion.xrel, event.motion.yrel);
                // if (!warpMouse) {
                //     delta = glm::vec2(0,0);
                // }
                break;
            }
        }
        ImGui_ImplSDL2_ProcessEvent(&event);
    }
    if (m_scripted) {
        for (auto &k : m_injectedKeys) {
            applyKey(k.first, k.second);
        }
        m_injectedKeys.clear();
        delta = m_injectedDelta;
        m_injectedDelta = glm::vec2(0);
        return;
    }

    if (warpMouse) {
        SDL_WarpMouseInWindow(display->getWindow(), display->getWidth()/2, display->getHeight()/2);
    }
}

void Input::setScripted(bool scripted) {
    m_scripted = scripted;
    delta = glm::vec2(0);
}

void Input::injectKey(int key, bool held) {
    m_injectedKeys.push_back(std::make_pair(key, held));
}

void Input::injectMouseDelta(const glm::vec2 &d) {
    m_injectedDelta = d;
}

void Input::applyKey(int key, bool held) {
    bool *in, *down, *up;
    int idx;
    if (key >= 0 && key <= 127) {
        idx = key; in = inputs; down = downKeys; up = upKeys;
    } else {
        idx = key - 1073741881;
        if (idx < 0 || idx >= NUM_SPECIAL_KEYS) return;
        in = specialInputs; down = specialDownKeys; up = specialUpKeys;
    }
    if (held && !in[idx]) down[idx] = true;
    if (!held && in[idx]) up[idx] = true;
    in[idx] = held;
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
const glm::vec2 &Input::getMousePos(){
    return mousePosition;
}
const glm::vec2 &Input::getCorrectedMousePos() {
    return correctedMousePosition;
}

const glm::vec2 &Input::getMouseDelta(){
    return delta;
}

const glm::vec2 &Input::getMouseDelta2(){
    return delta2;
}
