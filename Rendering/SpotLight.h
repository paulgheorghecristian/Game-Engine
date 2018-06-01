#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include "Light.h"

class SpotLight : public Light
{
    public:
        SpotLight(const Transform &transform,
                  const glm::vec3 &color,
                  bool casts_shadow = true);
        ~SpotLight();

        void render(Shader &shader) override;
        void render() override;

        void recomputeShadowMapViewMatrix() override;
        void recomputeShadowMapProjectionMatrix() override;

        static Shader &getLightAccumulationShader();
        static Mesh &getLightMesh();

    protected:

    private:
};

#endif // SPOTLIGHT_H
