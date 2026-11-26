#ifndef ROOMALIVE_H
#define ROOMALIVE_H

#include <glm/glm.hpp>
#include <vector>

#include "Aftermath.h"
#include "rapidjson/document.h"

class FrameBuffer;
class Mesh;
class PointLight;
class Shader;
class Timeline;

/* The room coming alive: its own element. Reality bending round a point in the room: what is near it
 * pulled in and twisted round it, breathing, a slow ripple running through walls and floor, a darkness
 * where it pulls, and a warm light glowing up through the floor. Drawn over the finished picture
 * (res/shaders/roomAlive.{vs,fs}, with the scene's depth: where each pixel is in the room), and its own
 * unshadowed light. Driven by its own timeline lanes; the map's "RoomAlive" block:
 *
 *   "RoomAlive": {"center": [x, y, z], "radius": 160, "darkRadius": 260,
 *                 "glowPosition": [x, y, z], "glowColor": [r, g, b], "glowRange": 1800}
 *
 * radius: how far from the centre reality bends (world units), fading out to it; darkRadius: how far
 * the darkness reaches. */
class RoomAlive
{
    public:
        /* its timeline lanes, joined to the list the timeline editor shows */
        static const std::vector<Aftermath::Parameter> &lanes();
        /* the map's "RoomAlive" block (none: it does nothing) */
        void load(const rapidjson::Value &map);
        /* once a frame: its lanes as the timeline has them now (and its light) */
        void readTimeline(const Timeline &timeline);
        /* over the finished picture (picture's first target), before the eyes and the anti-aliasing */
        void render(FrameBuffer &picture, unsigned int depthTexture, const glm::mat4 &viewProjection,
                    int width, int height, float time);

    private:
        bool m_has = false;
        glm::vec3 m_center = glm::vec3(0.0f), m_glowPosition = glm::vec3(0.0f), m_glowColor = glm::vec3(1.0f, 0.62f, 0.3f);
        float m_radius = 160.0f, m_darkRadius = 260.0f, m_glowRange = 1800.0f;
        float m_warp = 0.0f, m_dark = 0.0f, m_glow = 0.0f;

        PointLight *m_light = nullptr;     /* owned by RenderingMaster's light list while it glows */
        FrameBuffer *m_copy = nullptr;
        Shader *m_shader = nullptr;
        Mesh *m_quad = nullptr;
        void updateLight();
};

#endif // ROOMALIVE_H
