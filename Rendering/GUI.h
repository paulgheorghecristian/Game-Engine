#ifndef GUI_H
#define GUI_H

#include <glm/glm.hpp>
#include "Transform.h"

class GUI
{
    public:
        GUI(const glm::vec2 &position = glm::vec2 (0),
            const glm::vec2 &scale = glm::vec2 (1),
            const glm::vec2 &rotation = glm::vec2 (0));
        virtual void update (void *amount) = 0;
        virtual void render () = 0;
        virtual ~GUI();
        static glm::mat4 projectionMatrix;
        static void init (int width, int height);

        const Transform &getTransform () const;
    protected:
        Transform m_transform;
    private:
};

#endif // GUI_H
