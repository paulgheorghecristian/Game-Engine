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
        void renderShadowCube(const std::vector<Entity *> &entities);
        inline float getRange() { return m_transform.getScale().x; }
    protected:

    private:
        static RenderingObject g_lightMesh;
        static Shader &getShadowShader();

        int m_shadowMapSize;
        GLuint m_shadowCube = 0, m_shadowFbo = 0;
};

#endif // POINTLIGHT_H
