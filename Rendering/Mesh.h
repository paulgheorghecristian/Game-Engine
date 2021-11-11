#ifndef MESH_H
#define MESH_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <limits>

struct Vertex{
    /* TODO add default arguments properly */
    glm::vec3 positionCoords;
    glm::vec3 normalCoords;
    glm::vec2 textureCoords;
    glm::vec3 tangent, biTangent;
    glm::vec4 bonesWeights;
    glm::vec4 bonesIdx;

    Vertex() : Vertex (glm::vec3(0),
                       glm::vec3(0),
                       glm::vec2(0)) {
        bonesWeights = glm::vec4(0);
        bonesIdx = glm::ivec4(0);
    }

    Vertex(glm::vec3 positionCoords,
           glm::vec3 normalCoords,
           glm::vec2 tc,
           glm::vec3 tangent = glm::vec3 (0),
           glm::vec3 biTangent = glm::vec3 (0)) {

        this->positionCoords = positionCoords;
        this->normalCoords = normalCoords;
        this->textureCoords = tc;
        this->tangent = tangent;
        this->biTangent = biTangent;
    }
    Vertex(glm::vec3 positionCoords) : Vertex(positionCoords, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)){}
    Vertex(glm::vec3 positionCoords, glm::vec3 normalCoords) : Vertex(positionCoords, normalCoords, glm::vec2(0.0f, 0.0f)){}
    Vertex(glm::vec3 positionCoords, glm::vec2 textureCoords) : Vertex(positionCoords, glm::vec3(0.0f, 1.0f, 0.0f), textureCoords){}
};

class RenderingObject;

class Mesh
{
    public:
        Mesh(const std::vector<Vertex> &vertices,
              const std::vector<unsigned int> &indices,
              bool prepare = true,
              bool willBeUpdated = false);
        Mesh(bool prepare = true, bool willBeUpdated = true);
        GLuint getVao();
        void draw();
        GLsizei getNumberOfTriangles();

        const std::vector<Vertex> &getVertices() { return m_vertices; }
        const std::vector<unsigned int> &getIndices() { return m_indices; }

        void prepareGPU();

        static Mesh *getRectangle();
        static Mesh *getSurface(int, int);
        static Mesh *getCircle(float, float, float, int);
        static Mesh *getStar(float, float, float, int);
        static Mesh *getRectangleYUp();
        static Mesh *getArrowMesh();
        static Mesh *getDome(int widthPoints, int heightPoints);
        static Mesh &getRectangleStatic();

        virtual ~Mesh();

        inline bool getIsGPUPrepared() { return m_isGPUPrepared; }

        void update(const std::vector<Vertex> &vertices,
                    const std::vector<unsigned int> &indices,
                    bool willBeUpdated = false);

    protected:
    private:
        enum VBOs{
            VERTEX = 0, INDEX, NUM_VBOS
        };
        GLuint vaoHandle;
        GLuint vboHandles[NUM_VBOS];
        
        GLsizei m_numberOfTriangles;
        std::vector<Vertex> m_vertices;
        std::vector<unsigned int> m_indices;
        bool m_willBeUpdated;
        bool m_isGPUPrepared;
};

#endif // MESH_H
