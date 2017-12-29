#ifndef LIGHT_H
#define LIGHT_H

#include "Transform.h"
#include "Mesh.h"
#include "Shader.h"
#include "FrameBuffer.h"
#include "Texture.h"
#include "Camera.h"

#define LIGHT_CUTOFF_OFFSET 50

class Light
{
    public:
        enum LightType {
            DIRECTIONAL = 1,
            POINT,
            SPOT
        };
        Light(LightType type, glm::vec3 color, const Transform &transform);
        void render(Shader &shader);
        static void setPointMesh (Mesh *pointMesh);
        static void setSpotMesh (Mesh *spotMesh);
        Transform &getTransform();
        Light::LightType getLightType();
        const glm::mat4 &getSpotLightPerspectiveMatrix();
        const glm::mat4 &getSpotLightViewMatrix();
        FrameBuffer &getDepthTextureFrameBufferForSpotLight();
        Texture &getDepthTextureForSpotLight();
        void recomputeSpotLightViewMatrix();
        virtual ~Light();
    protected:
    private:
        LightType m_type;
        Transform m_transform;
        glm::vec3 m_lightColor;

        glm::mat4 m_spotLightPerspectiveMatrix, m_spotLightViewMatrix;
        FrameBuffer m_depthTextureFrameBufferForSpotLight;
        Texture m_depthTextureForSpotLight;

        static Mesh *pointMesh;
        static Mesh *spotMesh;
};

#endif // LIGHT_H
