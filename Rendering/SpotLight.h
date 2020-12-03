#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include "Light.h"

class SpotLight : public Light
{
    public:
        SpotLight(const Transform &transform,
                  const glm::vec3 &color,
                  bool casts_shadow = true,
                  bool volumetric = false);
        ~SpotLight();

        void update() override;

        void render(Shader &shader) override;
        void render() override;
        void renderGUI() override;
        void prepareOpenGLForLightPass() override;
        void prepareOpenGLForStencilPass() override;

        void recomputeShadowMapViewMatrix() override;
        void recomputeShadowMapProjectionMatrix() override;

        bool isVolumetric() { return m_volumetric; }

        static Shader &getLightAccumulationShader();
        static Mesh &getLightMesh();

    protected:

    private:
        bool m_volumetric;
        glm::vec3 worldRot;
};

#endif // SPOTLIGHT_H
