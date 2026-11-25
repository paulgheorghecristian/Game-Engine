#ifndef WATER_RENDERER_H
#define WATER_RENDERER_H

#include <cmath>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "rapidjson/document.h"
#include "FrameBuffer.h"
#include "Shader.h"
#include "Texture.h"

class Entity;
class Mesh;

/*
 * Planar water, ported from World2's WaterRenderer (DuDv-distorted
 * reflection/refraction + normal map) and adapted to the deferred pipeline:
 *
 *  1. renderReflection(): the sky and the scene above the water plane are
 *     rendered from a camera mirrored across the plane into a half
 *     resolution target (simple forward lighting, clip plane at the surface).
 *  2. render(): after lighting/compositing, the lit scene is copied (that is
 *     the refraction) and the water surface is drawn over it: distorted
 *     reflection/refraction blended by Fresnel, depth based murk and soft
 *     shorelines (manual depth test against the G-buffer depth), specular
 *     glints from the moon and every nearby light, fog, and particles /
 *     volumetric light composited back on top.
 *
 * Configured by a map's "Water" object:
 *   height, center [x,z], size [x,z], tiling, waveStrength, waveSpeed,
 *   waveHeight, shallowColor, deepColor, murkDepth, softEdge, reflectivity
 */
class WaterRenderer
{
    public:
        WaterRenderer(const rapidjson::Value &json, int renderWidth, int renderHeight);
        ~WaterRenderer();

        WaterRenderer(const WaterRenderer &) = delete;
        WaterRenderer &operator=(const WaterRenderer &) = delete;

        void renderReflection(const std::vector<Entity *> &entities);
        /* draws the water into `target` (color attachment 0, render size) */
        void render(FrameBuffer &target);

        inline float getHeight() const { return m_height; }
        /* whether (x, z) is over the water's rectangle */
        inline bool covers(float x, float z) const {
            return std::abs(x - m_center.x) <= m_size.x * 0.5f && std::abs(z - m_center.y) <= m_size.y * 0.5f;
        }

        /* lights passed to forward shaders (water surface, reflection pass) */
        static const int MAX_FORWARD_LIGHTS = 16;
        static void uploadForwardLights(Shader &shader, const glm::vec3 &referencePoint);
        /* the moon / sun, ambient and fog, for forward shaders (the reflection pass's) */
        static void uploadEnvironment(Shader &shader);

    private:
        float m_height;
        glm::vec2 m_center, m_size;
        float m_tiling, m_waveStrength, m_waveSpeed, m_waveHeight;
        glm::vec3 m_shallowColor, m_deepColor;
        float m_murkDepth, m_softEdge, m_reflectivity;
        float m_reflectionGlow;  /* "reflectionGlow": soft streaks under bright reflections */
        bool m_reflectionValid;

        int m_width, m_height_px;
        FrameBuffer m_reflectionFB;
        FrameBuffer m_sceneCopyFB;
        /* reflected particles (billboards, flames, sparks) kept apart from the
         * reflected scene, as the main pipeline does: they are composited over
         * the water (not dimmed by the Fresnel term) and bloomed afterwards */
        FrameBuffer m_reflectionParticlesFB;
        Texture m_reflectionTexture, m_sceneCopyTexture, m_reflectionParticlesTexture;
        Texture m_dudvTexture, m_normalTexture;

        bool m_reflectionMipmapsSet = false;
        Shader m_waterShader;
        Shader m_reflectionShader;
        Mesh *m_mesh;
        /* 1x1 "nothing in front" depth for particles drawn into the reflection
         * (they rely on the hardware depth test there instead) */
        GLuint m_farDepthTexture = 0;
        glm::mat4 m_modelMatrix;
};

#endif // WATER_RENDERER_H
