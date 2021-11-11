#ifndef BARGUI_H
#define BARGUI_H

#include "GUI.h"
#include "Text.h"

class Shader;
class Mesh;

class BarGUI : public GUI
{
    public:
        BarGUI(const glm::vec2 &position,
               const glm::vec2 &scale,
               const glm::vec3 &color,
               float maxValue,
               bool increasing,
               const glm::vec2 &rotation = glm::vec2 (0),
               const std::string &name = "no name");
        ~BarGUI();

        void update (double amount);
        void update (int amount);
        void render () override;
        static Shader &getBarGUIShader();
        static Mesh &getBarGUIMesh();
    protected:
        void update (void *amount) override;
    private:
        glm::vec3 m_color;
        Text amountText, barNameText;
        float m_maxValue;
        bool m_increasing;
};

#endif // BARGUI_H
