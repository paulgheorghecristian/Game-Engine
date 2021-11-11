#include "SlideButton.hpp"
#include "RenderingMaster.h"

#include "Mesh.h"

SlideButton::SlideButton(float x, float y, float value,
                        const std::string &text,
                        const glm::vec3 &color,
                        const std::function<void(Button *)> &onPressF,
                        const std::function<void(Button *)> &onReleaseF,
                        const std::function<void(Button *)> &onHoverF,
                        const std::function<void(Button *)> &onUnhoverF,
                        const std::function<void(Button *)> &onHoldF,
                        std::size_t size):
                        Button(x, y, 0.0f, 0.0f, text, color),
                        m_onPressF(onPressF),
                        m_onReleaseF(onReleaseF),
                        m_onHoverF(onHoverF),
                        m_onUnhoverF(onUnhoverF),
                        m_onHoldF(onHoldF),
                        m_slideText(Font::getFontFromCache("res/fonts/horror"),
                                    glm::vec3 (x, y, -10),
                                    color),
                        m_value(value) {
    m_slideText.changeSize(size);
    m_slideText.display(text);

    m_width = m_slideText.getCurrentWorldWidth();
    m_height = m_slideText.getCurrentWorldHeight();

    m_barTransform.setPosition(glm::vec3(x+m_width+120.0f, y-m_height*0.3f, -10.0f));
    m_barTransform.setRotation(glm::vec3(0.0f));
    m_barTransform.setScale(glm::vec3(200.0f, m_height*0.5f, 1.0f));

    float barLength = m_barTransform.getScale().x;
    float barStartPos = m_barTransform.getPosition().x - barLength*0.5f; 

    m_sliderTransform.setPosition(glm::vec3(barStartPos+barLength*m_value, y-m_height*0.3f, -10.0f));
    m_sliderTransform.setRotation(glm::vec3(0.0f));
    m_sliderTransform.setScale(glm::vec3(10.0f, m_height*2.0f, 1.0f));

    m_sliderColor = color * 0.4f;

    m_width = m_barTransform.getScale().x;
    m_height = m_sliderTransform.getScale().y;
    m_x = m_barTransform.getPosition().x - m_width * 0.5f;
    m_y = m_barTransform.getPosition().y + m_height * 0.5f;

    m_buttonType = ButtonType::SLIDER;
}

void SlideButton::onPress() {
    m_onPressF(this);
}

void SlideButton::onRelease() {
    m_onReleaseF(this);
}

void SlideButton::onHover() {
    m_onHoverF(this);
}

void SlideButton::onUnhover() {
    m_onUnhoverF(this);
}

void SlideButton::onHold() {
    m_onHoldF(this);
}

void SlideButton::input(Input &inputM) {
    Button::input(inputM);

    currentMouseDelta = inputM.getMouseDelta2();
    currentMousePos = inputM.getMousePos();
}

void SlideButton::update() {
    const glm::vec3 &sliderPosition = m_sliderTransform.getPosition();
    const glm::vec3 &barPosition = m_barTransform.getPosition();
    const glm::vec3 &barScale = m_barTransform.getScale();

    if (sliderPosition.x <= barPosition.x - barScale.x * 0.5f) {
        float newX = barPosition.x - barScale.x * 0.5f;

        m_sliderTransform.setPosition(glm::vec3(newX, sliderPosition.y, sliderPosition.z));
    }

    if (sliderPosition.x >= barPosition.x + barScale.x * 0.5f) {
        float newX = barPosition.x + barScale.x * 0.5f;

        m_sliderTransform.setPosition(glm::vec3(newX, sliderPosition.y, sliderPosition.z));
    }
}

void SlideButton::render() {
    Shader &shader = Button::getButtonShader();

    m_slideText.draw(RenderingMaster::simpleTextShader);

    shader.updateUniform("modelMatrix", (void *) &m_barTransform.getModelMatrix());
    shader.updateUniform("projectionMatrix", (void *) &RenderingMaster::orthoProjectionMatrix);
    shader.updateUniform("color", (void *) &m_color);

    shader.bind();
    Mesh::getRectangleStatic().draw();
    shader.unbind();

    shader.updateUniform("modelMatrix", (void *) &m_sliderTransform.getModelMatrix());
    shader.updateUniform("color", (void *) &m_sliderColor);

    shader.bind();
    Mesh::getRectangleStatic().draw();
    shader.unbind();
}
