#ifndef BLACK_HOLE_H
#define BLACK_HOLE_H

#include <cmath>
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
 *   (degrees), glow (strength of the light it casts, 0 = none), colorShift
 *   (0..1: how much the Doppler and gravitational shifts change the disk's
 *   color, not only its brightness: bluer where it comes towards the viewer,
 *   redder where it recedes and near the horizon)
 *
 * "sky": true makes it a sky object like the moon: placed by "direction"
 * [x,y,z] and "angularRadius" (horizon, radians) instead of a position and a
 * radius, it stays at the same place in the sky however the camera moves
 * (no parallax; the landscape is always in front of it). "moonStream" > 0
 * draws a stream of moon dust torn off the moon spiralling into its disk.
 * The clouds pass in front of a sky black hole (they are not lensed), and
 * "cloudClear" (0..1) thins them out around it so it is never buried.
 */
class BlackHole
{
    public:
        explicit BlackHole(const rapidjson::Value &json);
        explicit BlackHole(const glm::vec3 &position);
        ~BlackHole();

        BlackHole(const BlackHole &) = delete;
        BlackHole &operator=(const BlackHole &) = delete;

        /* draws into the bound framebuffer; sourceTexture is a copy of it.
         * lensSky (sky black holes): the sky behind it without clouds, as seen
         * by lensSkyViewProjection(); everything it bends comes from there */
        void render(GLuint sourceTexture, GLuint depthTexture, GLuint skyCube, int width, int height,
                    GLuint lensSky = 0);
        /* the same, seen by another camera (the water's mirrored one) */
        void render(GLuint sourceTexture, GLuint depthTexture, GLuint skyCube, int width, int height,
                    GLuint lensSky, const glm::mat4 &view, const glm::vec3 &eye);
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
        float colorShift = 0.7f;   /* Doppler / gravitational color shift (0 = none, 1 = physical) */
        glm::vec2 diskTilt = glm::vec2(12.0f, 6.0f);
        float glow = 1.0f;

        bool sky = false;
        glm::vec3 direction = glm::vec3(0.0f, 0.5f, -1.0f);
        float angularRadius = 0.045f;
        float moonStream = 0.0f;
        float cloudClear = 0.7f;   /* sky: how much the clouds thin out around it (0..1) */
        /* sky: a Kugelblitz eating the moon from the inside (it sits at the
         * moon, which the sky shader breaks up; see skyShader.fs). collapse
         * runs 0..1 through the stages, at collapseSpeed per minute */
        bool kugelblitz = false;
        float collapse = 0.0f;
        float collapseSpeed = 0.0f;
        float collapseDelay = 0.0f;    /* seconds the moon stays intact after Play, before ignition */
        bool autoPlay = false;         /* start playing when the map loads */
        float quitAfter = 0.0f;        /* seconds after the hole forms (into the storm) to fade out and close the game; 0: never */
        /* runtime playback (Play / Pause / Restart / Reset in the editor):
         * collapse only advances while playing */
        bool playing = false;
        float delayElapsed = 0.0f;
        float playTime = 0.0f;         /* the Kugelblitz's own clock, from Play / Restart / Reset */
        glm::vec2 collapseSite = glm::vec2(0.32f, 0.22f);   /* the AI's facility, on the moon's disc */
        /* runtime: the ignition flash is an event (collapse leaving 0), not a
         * stage you can stop on; it fades in real time */
        float ignitionTime = -1e9f;
        float previousCollapse = -1.0f;
        /* how far it has grown: 0 while it is inside the intact moon (and
         * too small to see), 1 when the moon is gone */
        float kugelblitzGrowth() const;
        /* the disk's normal (from diskTilt) and its outer edge as an angle on
         * the sky (sky objects; used by the sky shader for the moon stream) */
        glm::vec3 diskNormal() const;
        float diskOuterAngle() const { return std::atan(std::tan(angularRadius) * diskOuter); }
        /* sky: the view of the sky behind it, centred on it (camera at the
         * origin), LENS_SKY_HALF_ANGLE radians each way */
        static constexpr float LENS_SKY_HALF_ANGLE = 0.5f;
        glm::mat4 lensSkyView() const;
        glm::mat4 lensSkyProjection() const;
        /* sky: whether anything it bends can be on screen for this camera */
        bool skyMayBeVisible(const glm::vec3 &forward, const glm::mat4 &projection) const;

    private:
        void updateLight();
        PointLight *m_light = nullptr;    /* owned by RenderingMaster's light list */
        static Shader &getShader();
};

#endif // BLACK_HOLE_H
