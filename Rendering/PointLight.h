#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "Light.h"
#include "RenderingObject.hpp"

class PointLight : public Light
{
    public:
        PointLight(const Transform &transform,
                   const glm::vec3 &color);
        ~PointLight();

        void render(Shader &shader) override;
        void render() override;
        void renderGUI() override;
        void prepareOpenGLForLightPass() override;
        void prepareOpenGLForStencilPass() override;

        static Shader &getLightAccumulationShader();
        static void setLightMesh(RenderingObject &&lightMesh);

        virtual std::string jsonifyAttribs();
    protected:

    private:
        static RenderingObject g_lightMesh;
};

#endif // POINTLIGHT_H
