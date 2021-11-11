#include "Button.hpp"

Button *Button::selected = nullptr;

Button::Button(float x, float y, float width, float height,
                const std::string &text, const glm::vec3 &color):
                m_x(x), m_y(y), m_width(width), m_height(height), m_text(text),
                m_color(color),
                hover(false),
                isHolding(false) {

}

void Button::input(Input &inputM) {
    const glm::vec2 &mousePos = inputM.getCorrectedMousePos();

    if (Button::selected != nullptr &&
        Button::selected != this)
        return;

    if (mousePos.x > m_x && mousePos.x < m_x + m_width &&
        mousePos.y < m_y && mousePos.y > m_y - m_height) {
        hover = true;
        if (inputM.getMouse(1) == true) {
            isHolding = true;
            Button::selected = this;
        }

        if (inputM.getMouseDown(1) == true) {
            onPress();
            Button::selected = this;
        } else if (inputM.getMouseUp(1) == true) {
            if (m_buttonType != ButtonType::SLIDER) {
                onRelease();
            } else {
                isHolding = false;
            }
            Button::selected = nullptr;
        } else {
            onHover();
        }
    } else {
        if (hover == true) {
            onUnhover();
            hover = false;
        }

        if (m_buttonType == ButtonType::SLIDER &&
            isHolding == true &&
            inputM.getMouseUp(1) == true) {
            onRelease();
            isHolding = false;
        }
        if (inputM.getMouseUp(1) == true) {
            Button::selected = nullptr;
        }
    }

    if (m_buttonType == ButtonType::SLIDER) {
        if (inputM.getMouse(1) == true && isHolding == true) {
            onHold();
            Button::selected = this;
        }
    }
}

Shader &Button::getButtonShader() {
    static Shader shader ("res/shaders/GUIShader.json");

    return shader;
}