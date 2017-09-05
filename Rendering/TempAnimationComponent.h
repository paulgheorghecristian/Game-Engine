#ifndef TEMPANIMATIONCOMPONENT_H
#define TEMPANIMATIONCOMPONENT_H

#include "Component.h"
#include "glm/glm.hpp"

class TempAnimationComponent : public Component
{
    public:
        TempAnimationComponent(glm::vec3 addPosition,
                               glm::vec3 addRotation,
                               glm::vec3 addScale);
        glm::vec3 m_addPosition, m_addRotation, m_addScale;
        void input();
        void update();
        void render();
        const unsigned int getFlag() const;
        virtual ~TempAnimationComponent();
    protected:

    private:
};

#endif // TEMPANIMATIONCOMPONENT_H
