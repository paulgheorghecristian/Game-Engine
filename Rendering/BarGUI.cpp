#include "BarGUI.h"

#include "Shader.h"
#include "Mesh.h"
#include "Font.h"
#include "RenderingMaster.h"

/* TODO remove hard coded values */
BarGUI::BarGUI(const glm::vec2 &position,
               const glm::vec2 &scale,
               const glm::vec3 &color,
               const glm::vec2 &rotation,
               const std::string &name) :   GUI (position, scale, rotation),
                                            m_color (color),
                                            barNameText (Font::getNormalFont (),
                                                         glm::vec3 (position.x - 20.0f, position.y+30.0f, -10),
                                                         color,
                                                         name),
                                            amountText (Font::getNormalFont (),
                                                        glm::vec3 (position.x - 20.0f, position.y - 10.0f, -10),
                                                        color)
{
}

Shader &BarGUI::getBarGUIShader() {
    static Shader shader ("res/shaders/barGUIShader.json");

    return shader;
}

Mesh &BarGUI::getBarGUIMesh() {
    static Mesh mesh ({Vertex(glm::vec3(0, 20, 0), glm::vec3(0, 1, 0), glm::vec2(0, 1)),
                       Vertex(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0)),
                       Vertex(glm::vec3(20, 0, 0), glm::vec3(0, 1, 0), glm::vec2(1, 0)),
                       Vertex(glm::vec3(20, 20, 0), glm::vec3(0, 1, 0), glm::vec2(1, 1))},
                      {0,1,3,1,2,3});

    return mesh;
}

void BarGUI::update (void *amount) {
    bool result = true;
    double msPassed = *(double *) amount;

    m_transform.setScale (glm::vec3 ((msPassed * 2.0), 0.5, 0));
    amountText.displayDouble (msPassed);

    assert (result);
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
