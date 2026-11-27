#ifndef BLACKHOLEGLOW_H
#define BLACKHOLEGLOW_H

#include <memory>

#include "Mesh.h"
#include "Shader.h"
#include <GL/glew.h>
#include <vector>

class BlackHole;
class FrameBuffer;
class Mesh;
class Shader;

/* The glow of a black hole's own light, as a lens spreads a very bright light (the "Airy" bloom of
 * Starless): the disk's hot inner edge and the ring by the shadow get a soft halo, wider the
 * brighter they are. While the holes draw, their shader also writes their own light (no sky, no
 * scenery) into a texture of its own, times each hole's "bloom"; it is then spread (several widths,
 * the wide ones fainter) and added to the picture, before bloom and tone mapping. Nothing at all when
 * no hole has bloom (the default). Drawn with res/shaders/blackHoleGlow.{json,fs}. */
class BlackHoleGlow
{
    public:
        ~BlackHoleGlow();
        /* any hole that glows this frame */
        static bool wanted(const std::vector<BlackHole *> &holes);
        /* once a frame before the holes draw: the glow texture cleared */
        void begin(int width, int height);
        /* in place of binding target for a hole's draw: its picture, and the glow beside it */
        void bind(FrameBuffer &target);
        /* after all the holes: the glow spread and added to target's picture */
        void finish(FrameBuffer &target);

    private:
        int m_width = 0, m_height = 0;
        GLuint m_texture = 0, m_fbo = 0;
        std::unique_ptr<Shader> m_shader;
        std::unique_ptr<Mesh> m_quad;
};

#endif // BLACKHOLEGLOW_H
