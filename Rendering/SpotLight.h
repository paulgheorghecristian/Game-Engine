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

        int getNumSamplePoints() { return numSamplePoints; }
        float getCoef1() { return coef1; }
        float getCoef2() { return coef2; }

        void setNumSamplePoints(int num) { numSamplePoints = num; }
        void setCoef1(float coef) { coef1 = coef; }
        void setCoef2(float coef) { coef2 = coef; }

        static Shader &getLightAccumulationShader();
        static Mesh &getLightMesh();

        virtual std::string jsonifyAttribs();

    protected:

    private:
        bool m_volumetric;
        glm::vec3 worldRot;

        int numSamplePoints;
        float coef1;
        float coef2;
};

#endif // SPOTLIGHT_H
