#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "Texture.h"

class RenderComponent : public Component
{
    public:
        RenderComponent(Mesh *mesh, Shader *shader, Texture *texture,
                        Texture *normalMapTexture, Texture *roughness, const Material &material);
        void input(Input &inputManager);
        void update();
        void render();
        void render (Shader *externShader);
        const unsigned int getFlag() const;
        Shader *getShader();
        std::string jsonify() override;

        virtual ~RenderComponent();

    protected:

    private:
        Mesh *mesh;
        Shader *shader;
        Texture *texture, *normalMapTexture, *roughness;
        Material material;
};

#endif // RENDERCOMPONENT_H
