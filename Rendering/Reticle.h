#ifndef RETICLE_H
#define RETICLE_H

#include <memory>

#include "Mesh.h"
#include "Shader.h"

/*
 * Where the player aims: a small faint dot in the middle of the screen; aimed at something that can
 * be pressed (Core/Interaction), it opens into a larger warm ring; a press makes it pulse. Drawn on
 * the window over the finished picture (res/shaders/reticle.fs).
 */
class Reticle
{
    public:
        /* aim: 0 nothing, 1 something within reach (eased; 0.3: near it); pulse: 1 just pressed, to 0;
         * shown: 0..1 (hidden while the player cannot act) */
        void draw(int width, int height, float aim, float pulse, float shown);

    private:
        std::unique_ptr<Shader> m_shader;
        std::unique_ptr<Mesh> m_quad;
};

#endif // RETICLE_H
