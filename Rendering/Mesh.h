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

    Vertex() : Vertex (glm::vec3(0),
                       glm::vec3(0),
                       glm::vec2(0)) {}

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

class Mesh
{
    public:
        Mesh (const std::vector<Vertex> &vertices,
              const std::vector<unsigned int> &indices,
              bool willBeUpdated = false);
        Mesh (bool willBeUpdated = false);
        GLuint getVao();
        void draw();
        GLsizei getNumberOfTriangles();
        static Mesh *getRectangle();
        static Mesh *loadObject(const std::string &filePath);
        static void loadObjectIntoVectors(const std::string &filePath,
                            std::vector<Vertex> &vertices, std::vector<unsigned int> &indices);
        static Mesh *getSurface(int, int);
        static Mesh *getCircle(float, float, float, int);
        static Mesh *getStar(float, float, float, int);
        static Mesh *getRectangleYUp();
        static Mesh *getArrowMesh();
        static Mesh *getDome(int widthPoints, int heightPoints);
        void update (const std::vector<Vertex> &vertices,
                     const std::vector<unsigned int> &indices);
        virtual ~Mesh();
    protected:
    private:
        enum VBOs{
            VERTEX, INDEX, NUM_VBOS
        };
        GLuint vaoHandle;
        GLuint vboHandles[NUM_VBOS];
        GLsizei numberOfTriangles;
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        bool willBeUpdated;

        static float _stringToFloat(const std::string &source);
        static unsigned int _stringToUint(const std::string &source);
        static int _stringToInt(const std::string &source);
        static void _stringTokenize(const std::string &source, std::vector<std::string> &tokens);
        static void _faceTokenize(const std::string &source, std::vector<std::string> &tokens);

        static void computeTangentAndBi(Vertex &v1, Vertex &v2, Vertex &v3);
};

#endif // MESH_H
