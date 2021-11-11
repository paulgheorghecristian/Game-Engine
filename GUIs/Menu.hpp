#ifndef _MENU_HPP_
#define _MENU_HPP_

#include "Button.hpp"
#include "Mesh.h"

#include <vector>
#include <tuple>
#include "glm/glm.hpp"

class Texture;

class Menu {
public:
    Menu(const std::string &path,
            int screenWidth, int screenHeight);
    ~Menu();
    void addButton(Button *button);

    void input(Input &input);
    void render();
    void update();

    static Shader &getSplashScreenShader();
    static Mesh &getSplashScreenMesh();
private:
    std::vector<Button *> m_buttons;
    Texture *splashScreen;
    glm::mat4 m_scale;
};

#endif /* _MENU_HPP_ */