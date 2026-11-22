#ifndef BLACK_HOLE_H
#define BLACK_HOLE_H

#include <GL/glew.h>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

class Shader;
class PointLight;

/*
 * A black hole that bends light: drawn as a post-process on the composited
 * (HDR) image, before bloom and tone mapping.
 *
 * For every pixel whose view ray passes through the hole's sphere of
 * influence, the ray is marched along a light-like Schwarzschild geodesic
 * (acceleration -1.5 rs L^2 / r^5, which puts the photon sphere at 1.5 rs):
 * rays that fall below the event horizon are black, rays crossing the
 * accretion disk pick up its light (hot inner edge, Doppler-brightened on the
 * side moving towards the viewer, turbulence orbiting faster further in), and
 * rays that escape show the scene behind the hole where they leave (or the
 * sky). Geometry in front of the hole is left untouched. An optional point
 * light lets the disk light up its surroundings.
 *
 * Configured by entries of a map's "BlackHoles" array (all optional except
 * position):
 *   position [x,y,z], radius (event horizon, world units), lensing (bending
 *   strength, 1 = physical), influence (lensing reach, in radii),
 *   diskInner / diskOuter (in radii), diskColor [r,g,b], diskBrightness,
 *   diskOpacity (0..1), diskSpeed (orbit speed), diskTilt [pitch, roll]
 *   (degrees), glow (strength of the light it casts, 0 = none)
 */
class BlackHole
{
    public:
        explicit BlackHole(const rapidjson::Value &json);
        explicit BlackHole(const glm::vec3 &position);
        ~BlackHole();

        BlackHole(const BlackHole &) = delete;
        BlackHole &operator=(const BlackHole &) = delete;

        /* draws into the bound framebuffer; sourceTexture is a copy of it */
        void render(GLuint sourceTexture, GLuint depthTexture, GLuint skyCube, int width, int height);
        /* editor controls (inside an ImGui window); false when "Delete" was pressed */
        bool renderGUI(int index);
        void save(rapidjson::Value &json, rapidjson::Document::AllocatorType &allocator) const;

        glm::vec3 position;
        float radius = 40.0f;
        float lensing = 1.0f;
        float influence = 9.0f;
        float diskInner = 2.6f, diskOuter = 7.5f;
        glm::vec3 diskColor = glm::vec3(1.0f, 0.62f, 0.3f);
        float diskBrightness = 3.0f;
        float diskOpacity = 0.85f;
        float diskSpeed = 1.0f;
        glm::vec2 diskTilt = glm::vec2(12.0f, 6.0f);
        float glow = 1.0f;

    private:
        void updateLight();
        glm::vec3 diskNormal() const;
        PointLight *m_light = nullptr;    /* owned by RenderingMaster's light list */
        static Shader &getShader();
};

#endif // BLACK_HOLE_H
