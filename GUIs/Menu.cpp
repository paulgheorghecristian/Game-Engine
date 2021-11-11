#include "Menu.hpp"
#include "RenderingMaster.h"
#include "Texture.h"

#include <glm/glm.hpp>

Menu::Menu(const std::string &splashScreenPath,
            int screenWidth, int screenHeight) {
    splashScreen = new Texture(splashScreenPath, 0);

    m_scale = glm::scale(glm::mat4(1.0), glm::vec3(screenWidth, screenHeight, 1));
}

Menu::~Menu() {
    for (auto b : m_buttons) {
        delete b;
    }
    delete splashScreen;
}

void Menu::addButton(Button *button) {
    m_buttons.push_back(button);
}

void Menu::input(Input &input) {
    for (auto b: m_buttons) {
        b->input(input);
    }
}

void Menu::render() {
    Shader &shader = Menu::getSplashScreenShader();

    shader.updateUniform("splashScreenSampler", 0);
    shader.updateUniform("modelMatrix", (void *) &m_scale);
    shader.updateUniform("projectionMatrix", (void *) &RenderingMaster::orthoProjectionMatrix);

    shader.bind();
    splashScreen->use(0);
    Menu::getSplashScreenMesh().draw();
    shader.unbind();

    for (auto b: m_buttons) {
        b->render();
    }
}

void Menu::update() {
    for (auto b: m_buttons) {
        b->update();
    }
}

Shader &Menu::getSplashScreenShader() {
    static Shader shader("res/shaders/SplashScreenShader.json");

    return shader;
}

Mesh &Menu::getSplashScreenMesh() {
    static Mesh mesh({Vertex(glm::vec3(0, 1, -10), glm::vec3(0, 1, 0), glm::vec2(0, 1)),
                       Vertex(glm::vec3(0, 0, -10), glm::vec3(0, 1, 0), glm::vec2(0, 0)),
                       Vertex(glm::vec3(1, 0, -10), glm::vec3(0, 1, 0), glm::vec2(1, 0)),
                       Vertex(glm::vec3(1, 1, -10), glm::vec3(0, 1, 0), glm::vec2(1, 1))},
                      {0,1,3,1,2,3}, true);

    return mesh;
}