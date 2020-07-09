#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include "Light.h"

class DirectionalLight : public Light
{
    public:
        DirectionalLight(const Transform &transform,
                         const glm::vec3 &color,
                         const glm::vec3 &lightDirection,
                         bool casts_shadow = true);
        ~DirectionalLight();

        void update();

        void render(Shader &shader) override;
        void render() override;
        void prepareOpenGLForLightPass() override;
        void prepareOpenGLForStencilPass() override;

        void recomputeShadowMapViewMatrix() override;
        void recomputeShadowMapProjectionMatrix() override;

        static Shader &getLightAccumulationShader();
        static Mesh &getLightMesh();

    protected:

    private:
        glm::vec3 m_lightDirection;
};

#endif // DIRECTIONALLIGHT_H
