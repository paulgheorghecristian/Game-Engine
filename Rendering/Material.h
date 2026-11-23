#ifndef MATERIAL_H
#define MATERIAL_H

#include "glm/glm.hpp"

#include <vector>

class Texture;

/* Terrain material: textures tiled in world space (not stretched over the
 * mesh UVs) and blended by slope / height: grass on flat ground, triplanar
 * normal-mapped rock on steep slopes, mud and soil along the shoreline. */
struct TerrainLayers {
    Texture *grass = nullptr, *grass2 = nullptr, *rock = nullptr, *rockNormal = nullptr;
    Texture *dirt = nullptr, *soil = nullptr;
    Texture *grassNormal = nullptr, *dirtNormal = nullptr, *soilNormal = nullptr;
    float normalStrength = 1.0f;
    float tileSize = 60.0f;      /* world units per texture repeat */
    float shoreHeight = 0.0f;    /* shoreline blend around this height */
    float variation = 0.0f;      /* 0..1: dry / lush patches of grass (0 = uniform) */
    float rockDetail = 0.0f;     /* 0..1: relief, strata and ledges on the rock (0 = plain) */
    float wetness = 0.0f;        /* 0..1: damp ground, darker and glossy, with puddles (0 = dry) */
    ~TerrainLayers();
};

class Material
{
    public:
        Material();

        Material &operator=(const Material &otherMaterial) = delete;
        Material(const Material &otherMaterial) = delete;

        Material &operator=(Material &&otherMaterial);
        Material(Material &&otherMaterial);

        /* independent copy (textures are re-created from their paths, so the
         * image data itself is shared through the texture cache) */
        Material *clone() const;

        /* takes ownership */
        void setTerrain(TerrainLayers *terrain) { m_terrain = terrain; }
        const TerrainLayers *getTerrain() const { return m_terrain; }

        void setAmbient(const glm::vec3 &ambient);
        const glm::vec3 &getAmbient () const;
        void setDiffuse(const glm::vec3 &diffuse);
        const glm::vec3 &getDiffuse () const;
        void setSpecular(const glm::vec3 &specular);
        const glm::vec3 &getSpecular () const;
        void setShininess(float s);
        float getShininess() const;
        void setNormalMapStrength(float s);
        float getNormalMapStrength() const;
        /* 0..1: how much the surface glows on its own (lanterns, crystals) */
        void setEmissive(float e) { m_emissive = e; }
        /* emissive only at night (lantern globes): scaled by s_nightFactor */
        void setEmissiveNightOnly(bool nightOnly) { m_emissiveNightOnly = nightOnly; }
        float getEffectiveEmissive() const { return m_emissiveNightOnly ? m_emissive * s_nightFactor : m_emissive; }
        /* 1 at night, 0 by day; set by the renderer every tick */
        static float s_nightFactor;
        /* scales the specular term (written to the roughness target); 1 = old behaviour */
        void setSpecularStrength(float s) { m_specularStrength = s; }
        /* PBR (maps with Environment "pbr": true) */
        void setRoughness(float r) { m_roughness = r; }
        float getRoughness() const { return m_roughness; }
        void setMetallic(float m) { m_metallic = m; }
        float getMetallic() const { return m_metallic; }
        float getSpecularStrength() const { return m_specularStrength; }
        float getEmissive() const { return m_emissive; }

        void setIsBlackAlpha(bool s);
        bool getIsBlackAlpha() const;
        /* glass: drawn see-through after the scene (RenderingMaster::renderGlass),
         * its diffuse colour the tint; not in the G-buffer or the shadows */
        bool isGlass() const { return m_glass; }
        void setGlass(bool glass) { m_glass = glass; }
        void setDisableCulling(bool s);
        bool getDisableCulling() const;

        void setDiffuseTexture(Texture *texture) { m_diffuseTexture = texture; }
        void setNormalTexture(Texture *texture) { m_normalTexture = texture; }
        void setRoughnessTexture(Texture *texture) { m_roughnessTexture = texture; }

        Texture *getDiffuseTexture() { return m_diffuseTexture; }
        Texture *getNormalTexture() { return m_normalTexture; }
        Texture *getRoughnessTexture() { return m_roughnessTexture; }

        virtual ~Material();

    protected:

    private:
        glm::vec3 m_ambient, m_diffuse, m_specular;
        float m_shininess, m_normalMapStrength;
        float m_emissive = 0.0f;
        bool m_emissiveNightOnly = false;
        float m_specularStrength = 1.0f;
        float m_roughness = 0.6f, m_metallic = 0.0f;
        TerrainLayers *m_terrain = nullptr;

        bool m_isBlackAlpha, m_disableCulling;
        bool m_glass = false;

        Texture *m_diffuseTexture;
        Texture *m_normalTexture;
        Texture *m_roughnessTexture;
};

#endif // MATERIAL_H
