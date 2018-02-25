#ifndef PARTICLERENDERER_H
#define PARTICLERENDERER_H

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include "Camera.h"

#define NUM_OF_BYTES_PER_INSTANCE 16 /* 4 floats per matrix * sizeof (float) (without timeAlive) */

template <typename P, unsigned int NUM_OF_PARTICLES = 1>
class ParticleRenderer
{
    public:
        ParticleRenderer();
        void update (Camera &camera);
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
        void insertParticleAttributesInBuffer(P &particle, int &offset);

        /* construct it only once (make it static in getRenderingShader function) */
        static Shader renderingShader;
};

/* TODO make this locally static */
template <typename P, unsigned int NUM_OF_PARTICLES>
Shader ParticleRenderer<P, NUM_OF_PARTICLES>::renderingShader;

template <typename P, unsigned int NUM_OF_PARTICLES>
ParticleRenderer<P, NUM_OF_PARTICLES>::ParticleRenderer() {
    glm::vec3 center (0, 10, 0); /* TODO remove hardcode */
    createVao();

    for (unsigned int i = 0; i < NUM_OF_PARTICLES; i++) {
        float d1 = (float)(rand()) / (RAND_MAX-1);
        float d2 = (float)(rand()) / (RAND_MAX-1);
        float d3 = (float)(rand()) / (RAND_MAX-1);
        glm::vec3 pos = glm::vec3(center.x-50, center.y, center.z-50);
        pos.x += d1 * 100.0f;
        pos.y += d2 * 6.0f;
        pos.z += d3 * 100.0f;
        particles.push_back (P (pos, glm::vec3(0, d2 * 50.0f, 0), glm::vec3((d1 + 2.0f) * 20.0f)));
    }
    matricesBuffer = new float[NUM_OF_PARTICLES * 5];
    renderingShader.construct ("res/shaders/particleRenderShader.json");
    /* TODO remove this hardcode */
    glm::mat4 proj = glm::perspective(glm::radians(75.0f), 16.0f/9.0f, 1.0f, 5000.0f);
    bool result = renderingShader.updateUniform ("particleSampler", 0);
    result &= renderingShader.updateUniform ("projectionMatrix", (void *) &proj);
    result &= renderingShader.updateUniform ("liveForInMs", (void *) &P::getLiveForInMs ());
    result &= renderingShader.updateUniform ("totalNumOfSubTxts", (void *) &P::getTexture().getTotalNumOfSubTxts ());
    result &= renderingShader.updateUniform ("numOfSubTxtsH", (void *) &P::getTexture().getNumOfSubTxtsH ());
    result &= renderingShader.updateUniform ("numOfSubTxtsW", (void *) &P::getTexture().getNumOfSubTxtsW ());
    result &= renderingShader.updateUniform ("subWidth", (void *) &P::getTexture().getSubWidth ());
    result &= renderingShader.updateUniform ("subHeight", (void *) &P::getTexture().getSubHeight());

    assert (result);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::update (Camera &camera) {
    const glm::mat4 viewMatrix = camera.getViewMatrix ();
    const glm::vec3 cameraPosition = camera.getPosition ();

    bool result = renderingShader.updateUniform ("viewMatrix", (void *) &viewMatrix);
    assert (result);

    for (P &particle : particles) {
        /* TODO remove this hardcode */
        particle.update (1000 / 500, camera);
    }

    std::sort (std::begin (particles),
               std::end (particles),
               [&] (P &p1, P &p2) {
                    return (p1.getDistanceToCamera () < p2.getDistanceToCamera ());
                });

    /* TODO create here thread for sorting after X configurable updates */
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::draw () {
    int offset = 0;

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (GL_FALSE);

    /* TODO make sure here the particles are sorted (wait for the thread in update) */

    for (unsigned int i = 0; i < particles.size (); i++) {
        insertParticleAttributesInBuffer (particles[i], offset);
    }
    updateVbo ();

    glBindVertexArray(vaoHandle);
    renderingShader.bind();
    P::getTexture().use();
    glDrawArraysInstanced(GL_POINTS, 0, 1, NUM_OF_PARTICLES);
    glBindVertexArray(0);

    glDisable (GL_BLEND);
    glDepthMask (GL_TRUE);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::createVao(){
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    createEmptyVbo();
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE + sizeof (float), (void *) 0);
    glVertexAttribDivisor(0, 1);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, NUM_OF_BYTES_PER_INSTANCE + sizeof (float), (void *) NUM_OF_BYTES_PER_INSTANCE);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::createEmptyVbo(){
    glGenBuffers(1, &vboHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferData(GL_ARRAY_BUFFER, (NUM_OF_BYTES_PER_INSTANCE + sizeof (float)) * NUM_OF_PARTICLES, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::insertParticleAttributesInBuffer(P &particle, int &offset) {
    const glm::vec3 &pos = particle.getInstaPosition ();
    const glm::vec3 &scale = particle.getScale ();

    matricesBuffer[offset++] = pos.x;
    matricesBuffer[offset++] = pos.y;
    matricesBuffer[offset++] = pos.z;
    matricesBuffer[offset++] = scale.x;
    matricesBuffer[offset++] = particle.getAliveForInMs ();
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::updateVbo(){
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (NUM_OF_BYTES_PER_INSTANCE + sizeof (float)) * NUM_OF_PARTICLES, (void *) matricesBuffer);
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
