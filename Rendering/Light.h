#ifndef LIGHT_H
#include <cstdint>
#define LIGHT_H

#include "Transform.h"
#include "Mesh.h"
#include "FrameBuffer.h"
#include "Texture.h"
#include "Camera.h"

#include "btGhostObject.h"
#include "PhysicsMaster.h"

class Shader;
class InputManager;

class Light
{
    public:
        Light(const Transform &transform,
              const glm::vec3 &color,
              int depthMapWidth, int depthMapHeight,
              bool casts_shadow = false,
              bool needs_stencil = true,
              bool grab = false);

        Light(const Transform &transform,
              const glm::vec3 &color,
              bool casts_shadow = false,
              bool needs_stencil = true,
              bool grab = false);
        virtual ~Light();

        virtual void update();
        virtual void input(Input &inputManager);
        virtual void render(Shader &shader) = 0;
        virtual void render() = 0;
        virtual void renderGUI();
        virtual void prepareOpenGLForLightPass() = 0;
        virtual void prepareOpenGLForStencilPass() = 0;

        virtual void recomputeShadowMapViewMatrix();
        virtual void recomputeShadowMapProjectionMatrix();

        Transform &getTransform();
        const glm::mat4 &getShadowMapProjectionMatrix();
        const glm::mat4 &getShadowMapViewMatrix();
        FrameBuffer &getShadowMapFrameBuffer();

        /* Shadow map caching: signature of (light matrices + shadow casters) the
         * shadow map was last rendered with; re-render only when it changes. */
        uint64_t m_shadowSignature = 0;
        bool m_shadowSignatureValid = false;
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

        inline glm::vec3 &getLightColor() {
            return m_lightColor;
        }

        void setLightColor(const glm::vec3 &light) {
            m_lightColor = light;
        }

        inline bool isToBeRemoved() {
            return m_to_be_removed;
        }

        inline const glm::vec3 &getAttDiffuse() { return m_att_diffuse; }
        inline const glm::vec3 &getAttSpecular() { return m_att_specular; }

        inline void setAttDiffuse(float x, float y, float z) { m_att_diffuse.x = x; m_att_diffuse.y = y; m_att_diffuse.z = z;}
        inline void setAttSpecular(float x, float y, float z) { m_att_specular.x = x; m_att_specular.y = y; m_att_specular.z = z;}

        std::string jsonify();
        virtual std::string jsonifyAttribs();

        /* Optional animation, from the map's light "Attributes":
         *   "flicker": 0..1 amplitude (fire), "flickerSpeed": rate
         *   "orbit": {"center":[x,y,z], "radius", "speed" (deg/s), "bob", "phase"}
         *   "spin": deg/s around the world Y axis (sweeping beams) */
        struct Animation {
            float flicker = 0.0f, flickerSpeed = 8.0f;
            bool orbit = false;
            glm::vec3 orbitCenter = glm::vec3(0.0f);
            float orbitRadius = 0.0f, orbitSpeed = 0.0f, orbitBob = 0.0f, orbitPhase = 0.0f;
            float spin = 0.0f;
            bool nightOnly = false;   /* fades out by day (day-night cycle) */
        };
        void setAnimation(const Animation &animation);
        inline bool isAnimated() const { return m_animated; }
        /* length of one update tick, set by the engine */
        static float s_tickSeconds;
        /* 1 at night, 0 by day; set by the renderer each tick */
        static float s_nightFactor;

    protected:
        Transform m_transform;
        glm::vec3 m_lightColor;
        btGhostObject *m_ghostObj;

        glm::mat4 m_shadowMapProjectionMatrix, m_shadowMapViewMatrix;
        FrameBuffer m_shadowMapFrameBuffer;
        Texture m_shadowMapTexture;

        bool m_casts_shadow, m_needs_stencil_test;
        bool m_can_be_grabbed;
        bool m_to_be_removed;

        bool showGUI;

        Animation m_animation;
        bool m_animated = false;
        float m_animationTime = 0.0f;
        glm::vec3 m_baseColor = glm::vec3(0.0f);
        glm::quat m_baseRotation = glm::quat(1, 0, 0, 0);
        void animate();
        unsigned int imguiID;

        glm::vec3 m_att_diffuse, m_att_specular;

        void initPhysics(const Transform& transform, bool grab);
    private:
        static unsigned int g_num_of_instances;
};

#endif // LIGHT_H
