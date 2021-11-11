#include "TextButton.hpp"
#include "RenderingMaster.h"

TextButton::TextButton(float x, float y,
                        const std::string &text,
                        const glm::vec3 &color,
                        const std::function<void(Button *)> &onPressF,
                        const std::function<void(Button *)> &onReleaseF,
                        const std::function<void(Button *)> &onHoverF,
                        const std::function<void(Button *)> &onUnhoverF,
                        std::size_t size):
                        Button(x, y, 0.0f, 0.0f, text, color),
                        m_onPressF(onPressF),
                        m_onReleaseF(onReleaseF),
                        m_onHoverF(onHoverF),
                        m_onUnhoverF(onUnhoverF),
                        m_buttonText(Font::getFontFromCache("res/fonts/horror"),
                                    glm::vec3 (x, y, -10),
                                    color) {
    m_buttonText.changeSize(size);
    m_buttonText.display(text);

    m_width = m_buttonText.getCurrentWorldWidth();
    m_height = m_buttonText.getCurrentWorldHeight();

    m_buttonType = ButtonType::TEXT;
}

void TextButton::onPress() {
    m_onPressF(this);
}

void TextButton::onRelease() {
    m_onReleaseF(this);
}

void TextButton::onHover() {
    m_onHoverF(this);
}

void TextButton::onUnhover() {
    m_onUnhoverF(this);
}

void TextButton::input(Input &inputM) {
    Button::input(inputM);
}

void TextButton::update() {

}

void TextButton::render() {
    m_buttonText.draw(RenderingMaster::simpleTextShader);
}
