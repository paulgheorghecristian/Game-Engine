#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include "Light.h"
#include "RenderingObject.hpp"

class SpotLight : public Light
{
    public:
        SpotLight(const Transform &transform,
                  const glm::vec3 &color,
                  bool casts_shadow = true,
                  bool volumetric = false,
                  int shadowMapSize = 2048);
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
        /* Beam model for volumetric spots. beamDensity > 0 switches from the
         * original solid look to a beam whose density falls off exponentially
         * along its length (beamFalloff = distance over which it drops by e),
         * integrated along the view ray. */
        float getBeamDensity() const { return m_beamDensity; }
        float getBeamFalloff() const { return m_beamFalloff; }
        void setBeamDensity(float d) { m_beamDensity = d; }
        void setBeamFalloff(float f) { m_beamFalloff = f; }
        /* Cone half-angle in radians. By default it is derived from the
         * light's scale (scale.x / 1000, the original convention, which ties
         * the beam width to its range); "coneAngle" sets it independently so a
         * narrow beam can reach far. The light volume must still enclose the
         * cone: scale.x >= 2 * scale.z * tan(coneAngle). */
        float getConeAngle() const {
            return m_coneAngle > 0.0f ? m_coneAngle : m_transform.getScale().x / 1000.0f;
        }
        void setConeAngle(float a) { m_coneAngle = a; }

        void setNumSamplePoints(int num) { numSamplePoints = num; }
        void setCoef1(float coef) { coef1 = coef; }
        void setCoef2(float coef) { coef2 = coef; }

        static Shader &getLightAccumulationShader();
        static void setLightMesh(RenderingObject &&lightMesh);

        virtual std::string jsonifyAttribs();

    protected:

    private:
        bool m_volumetric;
        glm::vec3 worldRot;

        int numSamplePoints;
        float m_beamDensity = 0.0f, m_beamFalloff = 250.0f, m_coneAngle = 0.0f;
        float coef1;
        float coef2;

        static RenderingObject g_lightMesh;
};

#endif // SPOTLIGHT_H
