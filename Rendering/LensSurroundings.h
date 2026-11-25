#ifndef LENS_SURROUNDINGS_H
#define LENS_SURROUNDINGS_H

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "Shader.h"

class Entity;
class BlackHole;

/*
 * What the lens of a black hole in the world needs from past the edges of the
 * screen. The lens bends what is seen towards the hole: by the edge of the
 * screen, with the hole just beyond it, the rays of the pixels there meet land
 * past the edge, which the screen has no image of (it showed the sky there: a
 * hill cut off along a line).
 *
 * So the land around the camera is drawn into a cube centred on it (colour,
 * and depth for how far away it is), simply lit like the water's reflection
 * and at low resolution, only the faces the lens of a hole on screen can
 * reach, and only while there is one. The hole's shader traces the rays that
 * leave the screen against it (BlackHole, blackHole.fs).
 */
class LensSurroundings
{
    public:
        static const int SIZE = 512;

        LensSurroundings();
        ~LensSurroundings();

        LensSurroundings(const LensSurroundings &) = delete;
        LensSurroundings &operator=(const LensSurroundings &) = delete;

        /* draws the faces the lens of any hole can reach this frame; false (nothing drawn) when
         * no hole's lens is on screen */
        bool render(const std::vector<Entity *> &entities, const std::vector<BlackHole *> &holes);

        /* drawn this frame: the faces drawn (bit i: face i, GL's order), else 0 */
        int faces() const { return m_faces; }
        GLuint colorCube() const { return m_color; }
        GLuint depthCube() const { return m_depth; }
        const glm::vec3 &center() const { return m_center; }
        float nearPlane() const { return m_near; }
        float farPlane() const { return m_far; }

    private:
        GLuint m_color = 0, m_depth = 0, m_fbo = 0;
        int m_faces = 0;
        glm::vec3 m_center = glm::vec3(0.0f);
        float m_near = 0.5f, m_far = 5000.0f;
        Shader m_shader;
};

#endif // LENS_SURROUNDINGS_H
