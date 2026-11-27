#ifndef POINTLIGHT_H
#define POINTLIGHT_H

#include "Light.h"
#include "RenderingObject.hpp"

class PointLight : public Light
{
    public:
        PointLight(const Transform &transform,
                   const glm::vec3 &color,
                   bool castShadow = false,
                   int shadowMapSize = 512);
        ~PointLight();

        void render(Shader &shader) override;
        void render() override;
        void renderGUI() override;
        void prepareOpenGLForLightPass() override;
        void prepareOpenGLForStencilPass() override;

        static Shader &getLightAccumulationShader();
        static void setLightMesh(RenderingObject &&lightMesh);

        virtual std::string jsonifyAttribs();

        /* Omnidirectional shadows: a depth cube map storing (distance to the
         * light / range), rendered with six 90 degree views. */
        void renderShadowCube(const std::vector<Entity *> &entities, unsigned faces = 0x3F);
        /* whether a sphere (its centre from the light, its radius) is in a cube face's 90 degree view */
        static bool inFace(const glm::vec3 &toCenter, float radius, int face);
        /* each face's casters as last drawn (only a face whose casters changed is drawn again) */
        uint64_t m_faceSignature[6] = {0, 0, 0, 0, 0, 0};
        bool m_faceSignaturesValid = false;
        inline float getRange() { return m_transform.getScale().x; }
        /* how a fitting sends its light out (a map light's "emission"): full within `full` degrees of
         * `axis`, fading to none at `none` (a fitting open below: down and to the sides, nothing up) */
        void setEmission(const glm::vec3 &axis, float full, float none);
        /* a tube, not a point (a map light's "tube": {"axis": [1, 0, 0], "length": 29}): lit from along its
         * length, its highlights streaks along it */
        void setTube(const glm::vec3 &axis, float length);
    protected:

    private:
        static RenderingObject g_lightMesh;
        static Shader &getShadowShader();

        int m_shadowMapSize;
        glm::vec3 m_emitAxis = glm::vec3(0.0f, -1.0f, 0.0f);
        glm::vec2 m_emitCos = glm::vec2(-3.0f);     /* cos(full), cos(none); below -1: every way */
        glm::vec3 m_tubeAxis = glm::vec3(1.0f, 0.0f, 0.0f);
        float m_tubeHalf = 0.0f;                    /* half its length; 0: a point */
        GLuint m_shadowCube = 0, m_shadowFbo = 0;
};

#endif // POINTLIGHT_H
