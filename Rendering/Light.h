#ifndef LIGHT_H
#define LIGHT_H

#include "Transform.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Texture.h"
#include "Camera.h"

class Shader;

class Light
{
    public:
        Light(const Transform &transform,
              const glm::vec3 &color,
              int depthMapWidth, int depthMapHeight,
              bool casts_shadow = false,
              bool needs_stencil = true);

        Light(const Transform &transform,
              const glm::vec3 &color,
              bool casts_shadow = false,
              bool needs_stencil = true);
        virtual ~Light();

        virtual void update();

        virtual void render(Shader &shader) = 0;
        virtual void render() = 0;
        virtual void prepareOpenGLForLightPass() = 0;
        virtual void prepareOpenGLForStencilPass() = 0;

        virtual void recomputeShadowMapViewMatrix();
        virtual void recomputeShadowMapProjectionMatrix();

        Transform &getTransform();
        const glm::mat4 &getShadowMapProjectionMatrix();
        const glm::mat4 &getShadowMapViewMatrix();
        FrameBuffer &getShadowMapFrameBuffer();
        Texture &getShadowMapTexture();

        inline bool isCastingShadow() {
            return m_casts_shadow;
        }
        inline void setCastShadow(bool flag) {
            m_casts_shadow = flag;
        }
        inline bool needsStencilTest() {
            return m_needs_stencil_test;
        }

        inline const glm::vec3 &getLightColor() {
            return m_lightColor;
        }

    protected:
        Transform m_transform;
        glm::vec3 m_lightColor;

        glm::mat4 m_shadowMapProjectionMatrix, m_shadowMapViewMatrix;
        FrameBuffer m_shadowMapFrameBuffer;
        Texture m_shadowMapTexture;

        bool m_casts_shadow, m_needs_stencil_test;
    private:
};

#endif // LIGHT_H
