#ifndef RENDERCOMPONENT_H
#define RENDERCOMPONENT_H

#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "Component.h"
#include "RenderingMaster.h"

class RenderingMaster;

class RenderComponent : public Component
{
    public:
        RenderComponent(Mesh * mesh, Shader * shader, const Material &material);
        void input();
        void update();
        void render();
        const unsigned int getFlag() const;
        Shader *getShader();
        virtual ~RenderComponent();

    protected:

    private:
        Mesh * mesh;
        Shader * shader;
        Material material;
};

#endif // RENDERCOMPONENT_H
