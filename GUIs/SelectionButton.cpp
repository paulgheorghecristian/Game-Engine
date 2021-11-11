#include "SelectionButton.hpp"
#include "RenderingMaster.h"

SelectionButton::SelectionButton(int x, int y,
                                const std::string &text,
                                const glm::vec3 &color,
                                const std::vector<std::string> &selectionTexts):
                                Button(x, y, 0.0f, 0.0f, text, color),
                                m_buttonText(Font::getFontFromCache("res/fonts/horror"),
                                            glm::vec3 (x, y+40, -10),
                                            color) {
    int lastX = x + text.size() * 17;
    for (std::size_t i = 1; i <= selectionTexts.size(); i++) {
        if (i > 1) {
            lastX += m_selectionButtons[i-2]->getText().getCurrentWorldWidth() + 15.0f;
        }
        m_selectionButtons.push_back(new TextButton(lastX, y, selectionTexts[i-1],
                                                    color,
                                                    [](Button *button) {},
                                                    [&](Button *button) {
                                                        for (auto b : m_selectionButtons) {
                                                            ((TextButton *) b)->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                                        }
                                                        ((TextButton *) button)->getText().setColor(glm::vec3(1.0, 0.0, 0.0));
                                                    },
                                                    [&](Button *button) {
                                                        const glm::vec3 &color = ((TextButton *) button)->getText().getColor();
                                                        if (color == glm::vec3(1.0, 1.0, 1.0)) {
                                                            ((TextButton *) button)->getText().setColor(glm::vec3(1.0, 0.5, 0.5));
                                                        }
                                                    },
                                                    [](Button *button) {
                                                        const glm::vec3 &color = ((TextButton *) button)->getText().getColor();
                                                        if (color == glm::vec3(1.0, 0.5, 0.5)) {
                                                            ((TextButton *) button)->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                                        }
                                                    },
                                                    15
        ));
    }

    m_buttonText.changeSize(20);
    m_buttonText.display(text);

    m_buttonType = ButtonType::SELECTION;
}

void SelectionButton::input(Input &inputM) {
    for (auto b : m_selectionButtons) {
        b->input(inputM);
    }
}

void SelectionButton::update() {
    for (auto b : m_selectionButtons) {
        b->update();
    }
}

void SelectionButton::render() {
    m_buttonText.draw(RenderingMaster::simpleTextShader);

    for (auto b : m_selectionButtons) {
        b->render();
    }
}
