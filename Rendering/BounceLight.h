#ifndef BOUNCELIGHT_H
#define BOUNCELIGHT_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>

#include "FrameBuffer.h"
#include "Shader.h"

class Mesh;

/*
 * Light bouncing between surfaces near each other, in screen space (res/shaders/bounceLight.fs): a
 * wall lit full on brightens the one beside it, the light under a lamp spills onto the ceiling,
 * a corner fills in. One bounce, from this frame's direct light, at half resolution, smoothed
 * without crossing edges (bounceLightBlur.fs); the composite adds it as diffuse light. What is
 * off the screen gives nothing (the map's ambient stands for it).
 *
 * The map's Environment: "bounceLight" (strength: 1 as the light really bounces; 0 off) and
 * "bounceRadius" (how far the rays look, world units).
 */
class BounceLight
{
    public:
        /* from this frame's depth, eye-space normals, light passes and albedo (emissive in its alpha) */
        void render(int width, int height, GLuint depth, GLuint normals, GLuint light, GLuint albedo,
                    const glm::mat4 &projection, float radius, bool pbr, float emissiveStrength, Mesh *quad);
        /* the result (half resolution), for the composite */
        GLuint texture() const;

    private:
        std::unique_ptr<FrameBuffer> m_gather, m_across, m_result;
        std::unique_ptr<Shader> m_shader, m_blur;
        int m_width = 0, m_height = 0;
        float m_frame = 0.0f;
};

#endif // BOUNCELIGHT_H
