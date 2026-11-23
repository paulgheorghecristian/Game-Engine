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
        bool isGlass() {
            const std::vector<Material *> &materials = renderingObject.getMaterials();
            return !materials.empty() && materials[0]->isGlass();
        }
        RenderComponent(RenderingObject &&renderingObject);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        std::string jsonify() override;

        inline RenderingObject& getRenderingObject() { return renderingObject; }
        /* entirely past the draw distance (the map's fog cut-off): skip drawing it */
        bool isBeyondDrawDistance(Transform &transform);

        virtual ~RenderComponent();

    protected:

    private:
        RenderingObject renderingObject;
    private:
        bool m_castsShadow = true;
};

#endif // RENDERCOMPONENT_H
