#ifndef _SLIDEBUTTON_HPP_
#define _SLIDEBUTTON_HPP_

#include "Button.hpp"
#include "Text.h"
#include "Font.h"
#include "Transform.h"

#include <functional>

class SlideButton : public Button {
public:
    SlideButton(float x, float y, float value,
            const std::string &text, const glm::vec3 &color,
            const std::function<void(Button *)> &onPressF,
            const std::function<void(Button *)> &onReleaseF,
            const std::function<void(Button *)> &onHoverF,
            const std::function<void(Button *)> &onUnhoverF,
            const std::function<void(Button *)> &onHoldF,
            std::size_t size = 20);

    void onPress();
    void onRelease();
    void onHover();
    void onUnhover();
    void onHold();

    void input(Input &input);
    void update();
    void render();

    inline Text &getText() { return m_slideText; }
    float getValue() { return m_value; }
    void setValue(float value) { m_value = value; }

    const glm::vec2 &getCurrentMouseDelta() { return currentMouseDelta; }
    const glm::vec2 &getCurrentMousePos() { return currentMousePos; }
    Transform &getSliderTransform() { return m_sliderTransform; }
    Transform &getBarTransform() { return m_barTransform; }
private:
    std::function<void(Button *)> m_onPressF;
    std::function<void(Button *)> m_onReleaseF;
    std::function<void(Button *)> m_onHoverF;
    std::function<void(Button *)> m_onUnhoverF;
    std::function<void(Button *)> m_onHoldF;

    Text m_slideText;
    Transform m_barTransform, m_sliderTransform;
    glm::vec3 m_sliderColor;
    float m_value;
    glm::vec2 currentMouseDelta, currentMousePos;
};

#endif /* _SLIDEBUTTON_HPP_ */