#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "Light.h"

class PointLight : public Light
{
    public:
        PointLight(const Transform &transform,
                   const glm::vec3 &color);
        ~PointLight();

        void render(Shader &shader) override;
        void render() override;

        static Shader &getLightAccumulationShader();
        static Mesh &getLightMesh();
    protected:

    private:
};

#endif // POINTLIGHT_H
