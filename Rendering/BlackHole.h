#ifndef BLACK_HOLE_H
#define BLACK_HOLE_H

#include <algorithm>
#include <cmath>
#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>

#include "rapidjson/document.h"

class Shader;

/*
 * A black hole in the sky that bends light: drawn as a post-process on the composited
 * (HDR) image, before bloom and tone mapping. In the sky only, like the moon: everything
 * is in front of it, so all it bends is the sky behind it, which is drawn for it on its
 * own. (Holes placed in the world were tried and taken out: bending the land from the
 * screen's flat picture tore hills and broke up at the screen's edge.)
 *
 * For every pixel whose view ray passes through the hole's sphere of
 * influence, the ray is marched along a light-like Schwarzschild geodesic
 * (acceleration -1.5 rs L^2 / r^5, which puts the photon sphere at 1.5 rs):
 * rays that fall below the event horizon are black, rays crossing the
 * accretion disk pick up its light (hot inner edge, Doppler-brightened on the
 * side moving towards the viewer, turbulence orbiting faster further in), and
 * rays that escape show the sky behind the hole where they leave. Geometry in
 * front of the hole (all the landscape) is left untouched.
 *
 * Configured by entries of a map's "BlackHoles" array, each with "sky": true (an entry
 * without it is skipped), "direction" [x,y,z] and "angularRadius" (horizon, radians): it
 * stays at the same place in the sky however the camera moves. All else optional:
 *   lensing (bending strength, 1 = physical), influence (lensing reach, in radii),
 *   diskInner / diskOuter (in radii), diskColor [r,g,b], diskBrightness,
 *   diskOpacity (0..1), diskSpeed (orbit speed), diskTilt [pitch, roll]
 *   (degrees), colorShift
 *   (0..1: how much the Doppler and gravitational shifts change the disk's
 *   color, not only its brightness: bluer where it comes towards the viewer,
 *   redder where it recedes and near the horizon), horizon (the true event
 *   horizon as a fraction of radius / angularRadius, 1 if not given: then
 *   that radius is the scale of the system, its disk and lensing, and a
 *   small horizon leaves the middle clear, the stars seen through it, as
 *   for a hole of the Moon's mass, far smaller than a pixel), coreGlow
 *   (brightness of the point of light where it feeds, 0 = none) and
 *   coreSize (its size, in radii), physical (true: as it really is: the light's paths exact near
 *   the hole too (without it rays there bend more than they would: a bigger shadow, the look the
 *   older maps were made with), the disk's light shifted exactly as by gas in circular orbits round
 *   a Schwarzschild hole, g = sqrt(1 - 3M/r) / (1 - Omega b_z), and its colour and brightness those
 *   of a blackbody at g times its temperature; then diskColor and colorShift are not used) and
 *   diskTemperature (kelvin, its hottest, for physical), diskSmoothness (0..1: from turbulent streaks
 *   to smooth soft bands), photonRing (the bright ring hugging the shadow, physical: its brightness,
 *   1 if not given, 0 = none), photonRingWidth (its width in pixels, 3 if not given) and bloom (a soft
 *   glow round its own bright light, wider the brighter, as a lens spreads it; 0 if not given = none)
 *
 * "moonStream" > 0
 * draws a stream of moon dust torn off the moon spiralling into its disk.
 * The clouds pass in front of a sky black hole (they are not lensed), and
 * "cloudClear" (0..1) thins them out around it so it is never buried.
 */
class BlackHole
{
    public:
        explicit BlackHole(const rapidjson::Value &json);
        /* one in the sky in that direction (the editor's "add") */
        explicit BlackHole(const glm::vec3 &towards);
        ~BlackHole();

        BlackHole(const BlackHole &) = delete;
        BlackHole &operator=(const BlackHole &) = delete;

        /* draws into the bound framebuffer; sourceTexture is a copy of it.
         * lensSky (sky black holes): the sky behind it without clouds, as seen
         * by lensSkyViewProjection(); everything it bends comes from there */
        void render(GLuint sourceTexture, GLuint depthTexture, GLuint skyCube, int width, int height,
                    GLuint lensSky);
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
        float horizon = 1.0f;      /* the true event horizon, as a fraction of radius (1: the radius is the horizon) */
        float coreGlow = 0.0f;     /* the point of light where it feeds (0: none) */
        float coreSize = 0.3f;     /* its size, in radii */
        bool physical = false;     /* the disk's light shifted exactly and blackbody-coloured (see above) */
        float diskTemperature = 4800.0f;   /* kelvin, its hottest (physical) */
        float diskSmoothness = 0.0f;   /* 0: turbulent streaks; 1: smooth soft bands, a settled disk */
        float photonRing = 1.0f;       /* the bright ring hugging the shadow: its brightness (physical; 0 = none) */
        float photonRingWidth = 3.0f;  /* and its width, in pixels */
        float bloom = 0.0f;            /* its own light's glow, as a lens spreads it (BlackHoleGlow; 0 = none) */

        bool sky = true;       /* always: a hole on the ground (a position in the world) is no longer an option */
        glm::vec3 direction = glm::vec3(0.0f, 0.5f, -1.0f);
        float angularRadius = 0.045f;
        float moonStream = 0.0f;
        float cloudClear = 0.7f;   /* sky: how much the clouds thin out around it (0..1) */
        /* sky: a Kugelblitz eating the moon from the inside (it sits at the
         * moon, which the sky shader breaks up; see skyShader.fs). collapse
         * runs 0..1 through the stages, at collapseSpeed per minute */
        bool kugelblitz = false;
        /* its sounds (Audio/KugelblitzSounds): the blast after the ignition, the booms of the collapse */
        std::string blastSound;
        std::vector<std::string> boomSounds;
        float collapse = 0.0f;
        float collapseSpeed = 0.0f;
        float collapseDelay = 0.0f;    /* seconds the moon stays intact after Play, before ignition */
        bool autoPlay = false;         /* start playing when the map loads */
        float quitAfter = 0.0f;        /* seconds after the hole forms (into the storm) to fade out and close the game; 0: never */
        bool silent = false;           /* no sound from the moon's destruction (none crosses space):
                                        * the first heard is its debris arriving (Core/Aftermath) */
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
        /* a Kugelblitz played from the start: its playTime when the last of the moon has fallen in
         * (the wait, then the collapse at collapseSpeed per minute; none set: 12 s) */
        float formationTime() const {
            float rate = collapseSpeed > 0.0f ? collapseSpeed / 60.0f : 1.0f / 12.0f;
            return collapseDelay + 1.0f / rate;
        }
        /* a timeline sets its collapse ("collapse"; Core/Aftermath): then it follows that, not its
         * own playback above, and drivenRate is how fast the curve rises (per second) */
        bool driven = false;
        float drivenRate = 0.0f;
        float formedAt = -1.0f;       /* playTime when the last of the moon fell in, seen playing (-1: not) */
        /* how far it has grown: 0 while it is inside the intact moon (and
         * too small to see), 1 when the moon is gone */
        float kugelblitzGrowth() const;
        /* whether render() draws anything at all (a Kugelblitz still inside the moon does not:
         * the sky behind it and the copy of the screen it bends are not needed then) */
        bool drawsAnything() const { return !(sky && kugelblitz && kugelblitzGrowth() < 0.02f); }
        /* a hole in the world: the sphere its rays are marched in (holds the disk and the bending) */
        float reach() const { return radius * std::max(influence, diskOuter * 1.05f); }
        /* the disk's normal (from diskTilt) and its outer edge as an angle on
         * the sky (sky objects; used by the sky shader for the moon stream) */
        glm::vec3 diskNormal() const;
        float diskOuterAngle() const { return std::atan(std::tan(angularRadius) * diskOuter); }
        /* sky: the view of the sky behind it, centred on it (camera at the
         * origin), LENS_SKY_HALF_ANGLE radians each way */
        static constexpr float LENS_SKY_HALF_ANGLE = 0.5f;
        /* a Kugelblitz: the default seconds (on its playback clock) from the last of the moon
         * falling in to its debris reaching the Earth: half the moon, flung
         * out as plasma at about a seventh of the speed of light (like real
         * black holes' jets), lands ~8 s after the flash is seen (see
         * Game/recursion/STORY.md): seconds of silence while it crosses. The
         * sky's debris shell fills the sky by then (skyShader.fs), as the
         * storm hits. A map's timeline can put the shell anywhere in time instead
         * ("debrisShell", Core/Aftermath): this is when it does not */
        static constexpr float DEBRIS_ARRIVAL = 8.0f;
        glm::mat4 lensSkyView() const;
        glm::mat4 lensSkyProjection() const;
        /* sky: whether anything it bends can be on screen for this camera */
        bool skyMayBeVisible(const glm::vec3 &forward, const glm::mat4 &projection) const;

    private:
        static Shader &getShader(bool physical = false);
        /* the colour and brightness of a blackbody against its temperature (a 256 x 1 texture over
         * log T, 300..60000 K, linear RGB; made once) */
        static GLuint blackbodyTexture();
};

#endif // BLACK_HOLE_H
