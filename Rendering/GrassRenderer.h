#ifndef GRASS_RENDERER_H
#define GRASS_RENDERER_H

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "rapidjson/document.h"
#include "Shader.h"

struct TerrainLayers;

/*
 * Grass blades around the camera, drawn into the G-buffer (so the sun, cloud
 * shadows, AO and fog light them like everything else) and bent by the wind.
 *
 * At load the terrain mesh is rasterized into a heightfield texture; the
 * blades are then placed entirely on the GPU: one instance per cell of a grid
 * anchored in the world around the camera (jittered, clumped, randomly turned),
 * skipped under water and on steep slopes, shrinking away towards the edge of
 * the radius. Their color comes from the terrain's own grass textures and
 * patchiness, so each blade matches the ground it grows from.
 *
 * Configured by a map's "Grass" object:
 *   radius, spacing, height [min, max], width, minHeight (no grass below, e.g.
 *   under water), maxSlope (0 flat .. 1 vertical), wind, clumping,
 *   clearings [[x, z, radius], ...] (bare ground, up to 8), albedo (color scale),
 *   farRadius / farSpacing: a cheap ring of single-triangle blades beyond the
 *   detailed grass, out to farRadius (0 = none), so the field does not end
 */
class GrassRenderer
{
    public:
        GrassRenderer(const rapidjson::Value &json, const std::string &terrainMeshPath,
                      const glm::mat4 &terrainModelMatrix, const TerrainLayers *terrainLayers);
        ~GrassRenderer();

        GrassRenderer(const GrassRenderer &) = delete;
        GrassRenderer &operator=(const GrassRenderer &) = delete;

        /* expects the G-buffer to be bound */
        void render();

        bool isValid() const { return m_heightTexture != 0; }

    private:
        bool buildHeightfield(const std::string &meshPath, const glm::mat4 &model);

        float m_radius, m_spacing, m_width, m_minHeight, m_maxSlope, m_wind, m_clumping, m_albedo;
        glm::vec2 m_bladeHeight;
        std::vector<glm::vec3> m_clearings;
        int m_grid;
        float m_farRadius, m_farSpacing;
        int m_farGrid = 0;

        const TerrainLayers *m_terrain;
        GLuint m_heightTexture = 0;
        glm::vec2 m_fieldOrigin, m_fieldSize;   /* world xz covered by the heightfield */
        float m_fieldCell = 1.0f;

        GLuint m_vao = 0, m_vbo = 0;
        int m_bladeVertices = 0;
        GLuint m_farVao = 0, m_farVbo = 0;   /* one triangle per far blade */
        void drawLayer(bool far);
        Shader m_shader;
};

#endif // GRASS_RENDERER_H
