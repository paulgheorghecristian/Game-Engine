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
        ParticleRenderer(const glm::mat4 &projectionMatrix,
                         int screenWidth,
                         int screenHeight);
        void update (Camera &camera);
        void draw ();
        Shader &getRenderingShader();
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
};

template <typename P, unsigned int NUM_OF_PARTICLES>
ParticleRenderer<P, NUM_OF_PARTICLES>::ParticleRenderer(const glm::mat4 &projectionMatrix,
                                                        int screenWidth,
                                                        int screenHeight) {
    /* TODO make a GUI and make these modifiable by the GUI */
    /* each particle system should have a json properties file */
    glm::vec3 Center (0, 0, 0); /* TODO remove hardcode */
    bool result = true;

    createVao();

    #define R 3.0f
    #define r 0.5f
    #define H 100.0f
    glm::vec3 center = Center;
    center.y += H;
    for (int i = 0; i < NUM_OF_PARTICLES; i++) {
        float x = (float) (rand()) / (RAND_MAX - 1);
        float z = (float) (rand()) / (RAND_MAX - 1);
        x *= (2.0f * R);
        z *= (2.0f * R);
        x -= R;
        z -= R;

        if (x*x + z*z >= R*R) {
            i--;
            continue;
        }

        glm::vec3 dir = glm::vec3 (x, 0, z) - glm::vec3 (Center.x, 0, Center.z);
        dir *= (R / r);
        glm::vec3 tmp = center + dir;
        glm::vec3 velocity =  glm::normalize (tmp - glm::vec3 (x, 0, z)) * 150.0f;

        P p (glm::vec3 (x, Center.y, z), velocity, glm::vec3 (5));
        p.simulate = false;
        p.msDelay = ((1000.0 / 200)) * i;
        p.msDelayCopy = p.msDelay;

        particles.push_back (std::move (p));
    }
    #undef R
    #undef r
    #undef H

    assert (particles.size () == NUM_OF_PARTICLES);

    matricesBuffer = new float[NUM_OF_PARTICLES * 5];

    result &= getRenderingShader().updateUniform ("particleSampler", 0);
    result &= getRenderingShader().updateUniform ("depthSampler", 3);
    result &= getRenderingShader().updateUniform ("projectionMatrix", (void *) &projectionMatrix);
    result &= getRenderingShader().updateUniform ("liveForInMs", (void *) &P::getLiveForInMs ());
    result &= getRenderingShader().updateUniform ("totalNumOfSubTxts", (void *) &P::getTexture().getTotalNumOfSubTxts ());
    result &= getRenderingShader().updateUniform ("numOfSubTxtsH", (void *) &P::getTexture().getNumOfSubTxtsH ());
    result &= getRenderingShader().updateUniform ("numOfSubTxtsW", (void *) &P::getTexture().getNumOfSubTxtsW ());
    result &= getRenderingShader().updateUniform ("subWidth", (void *) &P::getTexture().getSubWidth ());
    result &= getRenderingShader().updateUniform ("subHeight", (void *) &P::getTexture().getSubHeight());
    result &= getRenderingShader().updateUniform ("screenHeight", (void *) &screenHeight);
    result &= getRenderingShader().updateUniform ("screenWidth", (void *) &screenWidth);

    assert (result);
}

template <typename P, unsigned int NUM_OF_PARTICLES>
void ParticleRenderer<P, NUM_OF_PARTICLES>::update (Camera &camera) {
    const glm::mat4 viewMatrix = camera.getViewMatrix ();
    const glm::vec3 cameraPosition = camera.getPosition ();

    bool result = getRenderingShader().updateUniform ("viewMatrix", (void *) &viewMatrix);
    assert (result);

    for (P &particle : particles) {
        /* TODO remove this hardcode */
        particle.update (1000.0f / 500, camera);
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
    glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (GL_FALSE);

    /* TODO make sure here the particles are sorted (wait for the thread in update) */

    for (unsigned int i = 0; i < particles.size (); i++) {
        insertParticleAttributesInBuffer (particles[i], offset);
    }
    updateVbo ();

    glBindVertexArray (vaoHandle);
    getRenderingShader().bind ();
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

template <typename P, unsigned int NUM_OF_PARTICLES>
Shader &ParticleRenderer<P, NUM_OF_PARTICLES>::getRenderingShader()
{
    static Shader shader ("res/shaders/particleRenderShader.json");

    return shader;
}

#endif // PARTICLERENDERER_H
