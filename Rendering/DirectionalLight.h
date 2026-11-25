#ifndef DIRECTIONALLIGHT_H
#define DIRECTIONALLIGHT_H

#include "Light.h"

#include <vector>
#include <glm/glm.hpp>

class DirectionalLight : public Light
{
    public:
        DirectionalLight(const Transform &transform,
                         const glm::vec3 &color,
                         const glm::vec3 &lightDirection,
                         bool casts_shadow = true);
        ~DirectionalLight();

        void update();
        inline const glm::vec3 &getLightDirection() const { return m_lightDirection; }
        inline void setLightDirection(const glm::vec3 &direction) { m_lightDirection = glm::normalize(direction); }

        void render(Shader &shader) override;
        void render() override;
        void prepareOpenGLForLightPass() override;
        void prepareOpenGLForStencilPass() override;

        void recomputeShadowMapViewMatrix() override;
        void recomputeShadowMapProjectionMatrix() override;

        static Shader &getLightAccumulationShader();
        /* the crowns the light pass casts soft shadows of (InstanceRenderComponent::appendCrownShadows),
         * set each frame; at most MAX_CROWNS */
        static void setCrownShadows(const std::vector<glm::vec4> &crowns);
        static const int MAX_CROWNS = 64;
        static Mesh &getLightMesh();

        virtual std::string jsonifyAttribs();

    protected:

    private:
        static std::vector<glm::vec4> s_crowns;
        glm::vec3 m_lightDirection;
};

#endif // DIRECTIONALLIGHT_H
