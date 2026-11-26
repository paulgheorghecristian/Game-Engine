#ifndef WAKING_H
#define WAKING_H

#include <glm/glm.hpp>
#include <vector>

#include "Aftermath.h"
#include "rapidjson/document.h"

class Camera;
class FrameBuffer;
class Mesh;
class Shader;
class Timeline;

/* Waking up (a map's opening scene): its own element. The eyes fighting to stay open (lids closing
 * from top and bottom), the picture blurred and doubled until it clears, and getting up: the camera
 * from lying in bed, to sitting up, to standing where the player starts, the player held until then.
 * Driven by its own timeline lanes (its own sequence file); the poses are the map's "Waking" block:
 *
 *   "Waking": {"lying":   {"position": [x, y, z], "yaw": deg, "pitch": deg},
 *              "sitting": {"position": [x, y, z], "yaw": deg, "pitch": deg}}
 *
 * standing is the map's PlayerStart. Drawn with res/shaders/waking.{vs,fs}, over the finished
 * picture just before its anti-aliasing. */
class Waking
{
    public:
        /* its timeline lanes, joined to the list the timeline editor shows */
        static const std::vector<Aftermath::Parameter> &lanes();

        /* the map's "Waking" block (none: it does nothing); standing: where the player starts */
        void load(const rapidjson::Value &map, const glm::vec3 &standing, float yaw, float pitch);
        /* once a frame: its lanes as the timeline has them now */
        void readTimeline(const Timeline &timeline);
        /* while getting up the player cannot move or look round (read by the player's input) */
        static bool holdsPlayer() { return s_holding; }
        /* once a frame, after the camera followed the player: the camera where the scene puts it */
        void applyCamera(Camera *camera, float time);
        /* over the finished picture (picture's first target), before its anti-aliasing */
        void render(FrameBuffer &picture, int width, int height, float time);

    private:
        struct Pose {
            glm::vec3 position = glm::vec3(0.0f);
            float yaw = 0.0f, pitch = 0.0f;     /* degrees */
        };
        bool m_has = false;
        Pose m_lying, m_sitting, m_standing;
        float m_eyesOpen = 1.0f, m_blur = 0.0f, m_getUp = 1.0f;
        bool m_wasHolding = false;
        static bool s_holding;

        FrameBuffer *m_copy = nullptr;
        Shader *m_shader = nullptr;
        Mesh *m_quad = nullptr;
};

#endif // WAKING_H
