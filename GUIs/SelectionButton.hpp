#ifndef _SELECTIONBUTTON_HPP_
#define _SELECTIONBUTTON_HPP_

#include "TextButton.hpp"
#include "Text.h"
#include "Font.h"

#include <functional>

class SelectionButton : public Button {
public:
    SelectionButton(int x, int y,
                    const std::string &text,
                    const glm::vec3 &color,
                    const std::vector<std::string> &selectionTexts);

    void input(Input &input);
    void update();
    void render();
private:
    std::vector<TextButton *> m_selectionButtons;
    std::vector<std::string> m_selectionTexts;
    Text m_buttonText;
};

#endif /* _SELECTIONBUTTON_HPP_ */