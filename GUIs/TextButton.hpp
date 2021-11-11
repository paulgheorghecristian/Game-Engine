#ifndef _TEXTBUTTON_HPP_
#define _TEXTBUTTON_HPP_

#include "Button.hpp"
#include "Text.h"
#include "Font.h"

#include <functional>

class TextButton : public Button {
public:
    TextButton(float x, float y,
            const std::string &text, const glm::vec3 &color,
            const std::function<void(Button *)> &onPressF,
            const std::function<void(Button *)> &onReleaseF,
            const std::function<void(Button *)> &onHoverF,
            const std::function<void(Button *)> &onUnhoverF,
            std::size_t size = 20);

    void onPress();
    void onRelease();
    void onHover();
    void onUnhover();

    void input(Input &input);
    void update();
    void render();

    inline Text &getText() { return m_buttonText; }
    
    friend class SelectionButton;
private:
    std::function<void(Button *)> m_onPressF;
    std::function<void(Button *)> m_onReleaseF;
    std::function<void(Button *)> m_onHoverF;
    std::function<void(Button *)> m_onUnhoverF;

    Text m_buttonText;
};

#endif /* _TEXTBUTTON_HPP_ */