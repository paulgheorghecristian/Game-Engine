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
};

#endif // RENDERCOMPONENT_H
