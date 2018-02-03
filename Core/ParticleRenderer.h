#ifndef PARTICLERENDERER_H
#define PARTICLERENDERER_H

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>

#define NUM_OF_BYTES_PER_INSTANCE 16 /* 4 floats per matrix * sizeof (float) */

template <typename P, unsigned int NUM_OF_PARTICLES = 1>
class ParticleRenderer
{
    public:
        ParticleRenderer();
        void update (const glm::mat4 &viewMatrix);
        void draw ();
        virtual ~ParticleRenderer();
    protected:
    private:
        std::vector<P> particles;
        GLuint vaoHandle, vboHandle;
        float *matricesBuffer;

        void createVao();
        void createEmptyVbo();
        void updateVbo();
        void insertPositionAndScaleInBuffer (const glm::vec3 &position,
                                             const glm::vec3 &scale,
                                             int &offset);

        static Shader renderingShader;
};

template <typename P, unsigned int NUM_OF_PARTICLES>
Shader ParticleRenderer<P, NUM_OF_PARTICLES>::renderingShader;

template <typename P, unsigned int NUM_OF_PARTICLES>
ParticleRenderer<P, NUM_OF_PARTICLES>::ParticleRenderer() {
    glm::vec3 center (0, 300, 0); /* TODO remove hardcode */
    createVao();

    for (unsigned int i = 0; i < NUM_OF_PARTICLES; i++) {
        float d1 = (float)(rand()) / (RAND_MAX-1);
        float d2 = (float)(rand()) / (RAND_MAX-1);
        float d3 = (float)(rand()) / (RAND_MAX-1);
        glm::vec3 pos = glm::vec3(center.x-3, center.y-3, center.z-3);
        pos.x += d1 * 6.0f;
        pos.y += d2 * 6.0f;
        pos.z += d3 * 6.0f;
        particles.push_back (P (pos, glm::vec3(0, 0, -d3 * 50), glm::vec3(d1 * 4.0f)));
    }
    matricesBuffer = new float[NUM_OF_PARTICLES * 4];
    renderingShader.construct ("res/shaders/particleRenderShader.json");
    glm::mat4 proj = glm::perspective(glm::radians(75.0f), 16.0f/9.0f, 1.0f, 1000.0f);
    bool result = renderingShader.updateUniform ("particleSampler", 0);
    result &= renderingShader.updateUniform ("projectionMatrix", (void *) &proj);
    assert (result);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::update (const glm::mat4 &viewMatrix) {
    int offset = 0;

    bool result = renderingShader.updateUniform ("viewMatrix", (void *) &viewMatrix);
    assert (result);

    for (P &particle : particles) {
        if (!particle.isAlive ()) {
            particle.reset ();
        } else {
            /* TODO remove this hardcode */
            particle.update (1000 / 60, viewMatrix);
        }
        insertPositionAndScaleInBuffer (particle.getInstaPosition(), particle.getScale(), offset);
    }

    updateVbo ();
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::draw () {
    glBindVertexArray(vaoHandle);
    renderingShader.bind();
    P::getTexture().use();
    glDrawArraysInstanced(GL_POINTS, 0, 1, NUM_OF_PARTICLES);
    glBindVertexArray(0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::createVao(){
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    createEmptyVbo();
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE, (void*)0);
    glVertexAttribDivisor(0, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::createEmptyVbo(){
    glGenBuffers(1, &vboHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferData(GL_ARRAY_BUFFER, NUM_OF_BYTES_PER_INSTANCE * NUM_OF_PARTICLES, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::insertPositionAndScaleInBuffer(const glm::vec3 &position,
                                                                           const glm::vec3 &scale,
                                                                           int &offset) {
    matricesBuffer[offset++] = position.x;
    matricesBuffer[offset++] = position.y;
    matricesBuffer[offset++] = position.z;
    matricesBuffer[offset++] = scale.x;
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::updateVbo(){
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_OF_BYTES_PER_INSTANCE*NUM_OF_PARTICLES, (void*) matricesBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
ParticleRenderer<P, NUM_OF_PARTICLES>::~ParticleRenderer()
{
    glDeleteBuffers(1, &vboHandle);
    glDeleteVertexArrays(1, &vaoHandle);

    delete[] matricesBuffer;
}

#endif // PARTICLERENDERER_H
