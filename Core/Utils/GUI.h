#ifndef GUI_H
#define GUI_H

#include <glm/glm.hpp>
#include "Transform.h"

class GUI
{
    public:
        GUI (const glm::vec2 position, const glm::vec2 scale, const glm::vec2 rotation);
        static void init (int width, int height);
        void render();
        void update();
        virtual ~GUI();
    protected:
        static glm::mat4 projectionMatrix;
        static Shader guiShader;
        Transform transform;
    private:
};

#endif // GUI_H
