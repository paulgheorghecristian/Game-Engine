#ifndef __DEBUG_RENDERER_HPP__
#define __DEBUG_RENDERER_HPP__

#include <glm/glm.hpp>
#include <GL/glew.h>
#include "Shader.h"

class DebugRenderer {
    public:
        static void init();
        static void destroy();
        static DebugRenderer *getInstance();

        void addLinePoint(const glm::vec3 &point);
        void clearLinePoints();
        void render();

        static Shader lineShader;

    private:
        GLuint vaoHandleLines;
        GLuint vboHandleLines;

        DebugRenderer();
        ~DebugRenderer();

        void renderLines();

        static DebugRenderer *m_instance;

        std::vector<glm::vec3> m_linePoints;
};

#endif /* __DEBUG_RENDERER_HPP__ */