#include "Mesh.h"
#include <iostream>

#include "RenderingObject.hpp"

Mesh::Mesh(const std::vector<Vertex> &vertices,
           const std::vector<unsigned int> &indices,
           bool prepare,
           bool willBeUpdated) : m_vertices(vertices), m_indices(indices) {
    m_numberOfTriangles = 0;
    vaoHandle = 0;
    vboHandles[VERTEX] = 0;
    vboHandles[INDEX] = 0;
    m_isGPUPrepared = false;

    if (prepare == true) {
        prepareGPU();
        m_willBeUpdated = true;
        update(vertices, indices, willBeUpdated);
        m_willBeUpdated = willBeUpdated;
    }
}

Mesh::Mesh(bool prepare, bool willBeUpdated) : m_willBeUpdated(willBeUpdated) {
    m_numberOfTriangles = 0;
    vaoHandle = 0;
    vboHandles[VERTEX] = 0;
    vboHandles[INDEX] = 0;
    m_isGPUPrepared = false;

    if (prepare == true) {
        prepareGPU();
    }
}

void Mesh::prepareGPU() {
    if (m_isGPUPrepared == true) {
        return;
    }
    //vao care retine starea meshei
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glGenBuffers(NUM_VBOS, vboHandles);

    glBindVertexArray(0);

    m_isGPUPrepared = true;
}

Mesh::~Mesh() {
    glDeleteBuffers(NUM_VBOS, vboHandles);
    glDeleteVertexArrays(1, &vaoHandle);
}

GLuint Mesh::getVao() {
    return vaoHandle;
}

GLsizei Mesh::getNumberOfTriangles() {
    return m_numberOfTriangles;
}

void Mesh::draw() {
    glBindVertexArray(getVao());
    glDrawElements(GL_TRIANGLES, m_numberOfTriangles, GL_UNSIGNED_INT, (void *) 0);
    glBindVertexArray(0);
}

void Mesh::update(const std::vector<Vertex> &vertices,
                    const std::vector<unsigned int> &indices,
                    bool willBeUpdated) {

    if (m_willBeUpdated == false) {
        std::cout << "WARNING, updating not supported!" << std::endl;
        return;
    }

    glBindVertexArray(vaoHandle);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[VERTEX]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(Vertex) * vertices.size(),
                 &vertices[0],
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[INDEX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof(unsigned int) * indices.size(),
                 &indices[0],
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    //ii descriu shader-ului datele trimise
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3)));
    glEnableVertexAttribArray (3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3) + sizeof (glm::vec2)));
    glEnableVertexAttribArray (4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(glm::vec3) + sizeof (glm::vec2)));

    m_numberOfTriangles = indices.size();

    glBindVertexArray(0);
}

// STATIC FUNCS FOR MESH CREATION
Mesh* Mesh::getRectangle() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(-1, 1, 0), glm::vec3(0, 1, 0), glm::vec2(0, 1)));
    vertices.push_back(Vertex(glm::vec3(-1, -1, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0)));
    vertices.push_back(Vertex(glm::vec3(1, -1, 0), glm::vec3(0, 1, 0), glm::vec2(1, 0)));
    vertices.push_back(Vertex(glm::vec3(1, 1, 0), glm::vec3(0, 1, 0), glm::vec2(1, 1)));

    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(3);

    return new Mesh(vertices, indices);
}

Mesh* Mesh::getRectangleYUp() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(-1, 0, 1), glm::vec3(0, 1, 0), glm::vec2(0, 0)));
    vertices.push_back(Vertex(glm::vec3(-1, 0, -1), glm::vec3(0, 1, 0), glm::vec2(0, 1)));
    vertices.push_back(Vertex(glm::vec3(1, 0, -1), glm::vec3(0, 1, 0), glm::vec2(1, 1)));
    vertices.push_back(Vertex(glm::vec3(1, 0, 1), glm::vec3(0, 1, 0), glm::vec2(1, 0)));

    indices.push_back(0);
    indices.push_back(3);
    indices.push_back(1);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);

    return new Mesh(vertices, indices);
}

Mesh *Mesh::getArrowMesh() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(-0.1f, 0, 1)));
    vertices.push_back(Vertex(glm::vec3(0.1f, 0, 1)));
    vertices.push_back(Vertex(glm::vec3(0.1f, 0, -0.6)));
    vertices.push_back(Vertex(glm::vec3(-0.1f, 0, -0.6)));

    vertices.push_back(Vertex(glm::vec3(-0.3f, 0, -0.6)));
    vertices.push_back(Vertex(glm::vec3(0, 0, -1)));
    vertices.push_back(Vertex(glm::vec3(0.3f, 0, -0.6)));

    indices.push_back (0);
    indices.push_back (1);
    indices.push_back (2);

    indices.push_back (0);
    indices.push_back (2);
    indices.push_back (3);

    indices.push_back (4);
    indices.push_back (6);
    indices.push_back (5);

    return new Mesh (vertices, indices);
}

Mesh* Mesh::getDome(int widthPoints, int heightPoints)
{
    float pi = 3.1415;
    float phiUpperBound = pi / 2.0;
    float thetaUpperBound = 2 * pi;

    int numOfPointsY = heightPoints;
    int numOfPointsXZ = widthPoints;

    float stepY = phiUpperBound / (float)(numOfPointsY-1);
    float stepXZ = thetaUpperBound / (float)(numOfPointsXZ-1);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for(int i = 0; i < numOfPointsXZ; i++){
        float theta = (float)i*stepXZ;
        for(int j = 0; j < numOfPointsY; j++){
            float phi = (float)j*stepY;

            float x = glm::cos(theta) * glm::sin(phi) * 0.5f;
            float z = glm::sin(theta) * glm::sin(phi) * 0.5f;
            float y = glm::cos(phi) * 0.5f;

            float u = theta / thetaUpperBound;
            float v = phi / phiUpperBound;

            vertices.push_back(Vertex(glm::vec3(x, y, z), glm::vec2(u, v)));

            int index = numOfPointsY * i + j;
            int nextIndex = (numOfPointsY * (i+1)) + j;

            if(j < numOfPointsY-1 && i < numOfPointsXZ-1){
                indices.push_back(index);
                indices.push_back(nextIndex);
                indices.push_back(index+1);

                indices.push_back(index+1);
                indices.push_back(nextIndex);
                indices.push_back(nextIndex+1);}
        }
    }

    int index = (numOfPointsXZ-1) * numOfPointsY;
    int index2 = (numOfPointsXZ * numOfPointsY);

    for(int i = 0; i < numOfPointsY-1; i++){
        float phi = (float)i*stepY;

        float x = glm::sin(phi) * 0.5f;
        float z = 0;
        float y = glm::cos(phi) * 0.5f;

        float v = phi / phiUpperBound;

        vertices.push_back(Vertex(glm::vec3(x, y, z), glm::vec2(1.0, v)));

        indices.push_back(index+i);
        indices.push_back(index2+i);
        indices.push_back(index+i+1);

        indices.push_back(index+i+1);
        indices.push_back(index2+i);
        indices.push_back(index2+i+1);
    }

    vertices.push_back(Vertex(glm::vec3(0.5, 0, 0), glm::vec2(1.0, 1.0)));

    return new Mesh(vertices, indices);
}

Mesh *Mesh::getSurface(int width, int height){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float dw = 2.0f / (float)width;
    float dh = 2.0f / (float)height;
    //creeaza suprafata pentru apa
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            int first_index = 4*(j + i*width);
            vertices.push_back(Vertex(glm::vec3(-1.0f + j * dw, 0.0f, 1.0f - i * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + j * dw, 0.0f, 1.0f - (i+1) * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + (j+1) * dw, 0.0f, 1.0f - (i+1) * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + (j+1) * dw, 0.0f, 1.0f - i * dh), glm::vec3(0, 1, 0)));

            indices.push_back(first_index+2);
            indices.push_back(first_index+1);
            indices.push_back(first_index+0);

            indices.push_back(first_index);
            indices.push_back(first_index+3);
            indices.push_back(first_index+2);

        }
    }
    return new Mesh(vertices, indices);
}

Mesh *Mesh::getCircle(float x, float y, float radius, int numOfTriangles){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(x, y, 0)));
    float step = 360.0f / numOfTriangles;

    int index = 1;

    for(float theta = 0; theta < 360.0f; theta += step){
        float _x1 = radius * glm::cos(glm::radians(theta)) + x;
        float _y1 = radius * glm::sin(glm::radians(theta)) + y;

        float _x2 = radius * glm::cos(glm::radians(theta + step)) + x;
        float _y2 = radius * glm::sin(glm::radians(theta + step)) + y;
        vertices.push_back(Vertex(glm::vec3(_x1, _y1, 0)));
        vertices.push_back(Vertex(glm::vec3(_x2, _y2, 0)));

        indices.push_back(index+1);
        indices.push_back(0);
        indices.push_back(index);
        index += 2;
    }

    return new Mesh(vertices, indices);
}

Mesh *Mesh::getStar(float x, float y, float radius, int numOfTriangles){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(x, y, 0)));
    float step = 360.0f / numOfTriangles;

    int index = 1;

    for(float theta = 0; theta < 360.0f; theta += 4.0f*step){
        float _x1 = radius * glm::cos(glm::radians(theta)) + x;
        float _y1 = radius * glm::sin(glm::radians(theta)) + y;

        float _x2 = radius * glm::cos(glm::radians(theta + step)) + x;
        float _y2 = radius * glm::sin(glm::radians(theta + step)) + y;
        vertices.push_back(Vertex(glm::vec3(_x1, _y1, 0)));
        vertices.push_back(Vertex(glm::vec3(_x2, _y2, 0)));

        indices.push_back(index+1);
        indices.push_back(0);
        indices.push_back(index);
        index += 2;
    }


    return new Mesh(vertices, indices);
}