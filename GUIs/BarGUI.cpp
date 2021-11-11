#include "BarGUI.h"

#include "Shader.h"
#include "Mesh.h"
#include "Font.h"
#include "RenderingMaster.h"

/* TODO remove hard coded values */
BarGUI::BarGUI(const glm::vec2 &position,
               const glm::vec2 &scale,
               const glm::vec3 &color,
               float maxValue,
               bool increasing,
               const glm::vec2 &rotation,
               const std::string &name) :   GUI (position, scale, rotation),
                                            m_color (color),
                                            barNameText(Font::getFontFromCache("res/fonts/normalFont"),
                                                         glm::vec3 (position.x - 20.0f, position.y+30.0f, -10),
                                                         color,
                                                         name),
                                            amountText(Font::getFontFromCache("res/fonts/normalFont"),
                                                        glm::vec3 (position.x - 20.0f, position.y - 10.0f, -10),
                                                        color),
                                            m_maxValue (maxValue),
                                            m_increasing (increasing)
{
}

Shader &BarGUI::getBarGUIShader() {
    static Shader shader ("res/shaders/GUIShader.json");

    return shader;
}

Mesh &BarGUI::getBarGUIMesh() {
    static Mesh mesh({Vertex(glm::vec3(0, 100, 0), glm::vec3(0, 1, 0), glm::vec2(0, 1)),
                       Vertex(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0)),
                       Vertex(glm::vec3(100, 0, 0), glm::vec3(0, 1, 0), glm::vec2(1, 0)),
                       Vertex(glm::vec3(100, 100, 0), glm::vec3(0, 1, 0), glm::vec2(1, 1))},
                      {0,1,3,1,2,3}, true);

    return mesh;
}

void BarGUI::update (void *amount) {
    bool result = true;
    double msPassed = *(double *) amount;

    float scaleFactor = msPassed / m_maxValue;

    m_transform.setScale (glm::vec3 (scaleFactor, 0.1, 0));
    if (m_increasing) {
        m_color.r = scaleFactor;
        m_color.g = 1.0f - scaleFactor;
    } else {
        m_color.r = 1.0f - scaleFactor;
        m_color.g = scaleFactor;
    }
    amountText.displayDouble (msPassed);

    assert (result);
}

void BarGUI::update (double amount) {
    BarGUI::update ((void *) &amount);
}

void BarGUI::update (int amount) {
    double amountD = (double) amount;
    BarGUI::update ((void *) &amountD);
}

void BarGUI::render () {
    bool result = true;
    Shader &shader = BarGUI::getBarGUIShader();

    result &= shader.updateUniform("modelMatrix", (void *) &m_transform.getModelMatrix());
    result &= shader.updateUniform("color", (void *) &m_color);

    shader.bind();
    BarGUI::getBarGUIMesh().draw();
    shader.unbind();

    amountText.draw (RenderingMaster::simpleTextShader);
    barNameText.draw (RenderingMaster::simpleTextShader);

    assert (result);
}

BarGUI::~BarGUI()
{
    //dtor
}
