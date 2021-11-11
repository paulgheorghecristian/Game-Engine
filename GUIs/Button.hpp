#ifndef _BUTTON_HPP_
#define _BUTTON_HPP_

#include "Input.h"
#include "Shader.h"

#include <string>
#include "glm/glm.hpp"

class Button {
public:
    enum ButtonType {
        TEXT = 0,
        SELECTION,
        SLIDER
    };
    Button(float x, float y, float width, float height,
            const std::string &text, const glm::vec3 &color);
    virtual void input(Input &inputM);

    virtual void onPress() {};
    virtual void onRelease() {};
    virtual void onHover() {};
    virtual void onUnhover() {};
    virtual void onHold() {};

    virtual void update()                   = 0;
    virtual void render()                   = 0;

    static Shader &getButtonShader();
protected:
    float m_x, m_y, m_width, m_height;
    std::string m_text;
    glm::vec3 m_color;

    ButtonType m_buttonType;
private:
    bool hover;
    bool isHolding;

    static Button *selected;
};

#endif /* _BUTTON_HPP_ */