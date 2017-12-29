#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "RenderingMaster.h"
#include "Texture.h"

class RenderingMaster;

class RenderComponent : public Component
{
    public:
        RenderComponent(Mesh *mesh, Shader *shader, Texture *texture, Texture *normalMapTexture, const Material &material);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        virtual ~RenderComponent();

    protected:

    private:
        Mesh *mesh;
        Shader *shader;
        Texture *texture, *normalMapTexture;
        Material material;
};

#endif // RENDERCOMPONENT_H
