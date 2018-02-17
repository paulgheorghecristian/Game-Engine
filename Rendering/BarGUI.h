#ifndef BARGUI_H
#define BARGUI_H

#include "GUI.h"
#include "Text.h"

#define START_X 1500
#define START_Y 100
#define Y_OFFSET 100

class Shader;
class Mesh;

class BarGUI : public GUI
{
    public:
        BarGUI(const glm::vec2 &position,
               const glm::vec2 &scale,
               const glm::vec3 &color,
               const glm::vec2 &rotation = glm::vec2 (0),
               const std::string &name = "no name");
        ~BarGUI();

        void update (void *amount) override;
        void render () override;
        static Shader &getBarGUIShader();
        static Mesh &getBarGUIMesh();
    protected:
    private:
        glm::vec3 m_color;
        Text amountText, barNameText;
};

#endif // BARGUI_H
