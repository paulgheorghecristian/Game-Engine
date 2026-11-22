#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "Texture.h"

#include "RenderingObject.hpp"

class RenderComponent : public Component
{
    public:
        /* "castShadow": false in the map JSON keeps glowing props (lamps, lantern
         * globes) out of shadow maps, so a light placed inside them still shines */
        inline void setCastsShadow(bool casts) { m_castsShadow = casts; }
        inline bool castsShadow() const { return m_castsShadow; }
        RenderComponent(RenderingObject &&renderingObject);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        std::string jsonify() override;

        inline RenderingObject& getRenderingObject() { return renderingObject; }

        virtual ~RenderComponent();

    protected:

    private:
        RenderingObject renderingObject;
    private:
        bool m_castsShadow = true;
};

#endif // RENDERCOMPONENT_H
