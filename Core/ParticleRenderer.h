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

enum BlendType {
    ADDITIVE_BLENDING = 0,
    ALPHA_BLENDING,
};

template <typename P>
class ParticleRenderer
{
    public:
        ParticleRenderer(const glm::mat4 &projectionMatrix,
                         int screenWidth,
                         int screenHeight,
                         rapidjson::Document &particleVolumePropertiesJson);
        void update (Camera &camera, double dt);
        void draw ();
        Shader &getRenderingShader();
        virtual ~ParticleRenderer();

    protected:
    private:
        std::vector<P> particles;
        GLuint vaoHandle, vboHandle;
        float *matricesBuffer;
        BlendType m_blendType;

        void createVao();
        void createEmptyVbo();
        void updateVbo();
        void insertParticleAttributesInBuffer(P &particle, int &offset);
};

template <typename P>
ParticleRenderer<P>::ParticleRenderer(const glm::mat4 &projectionMatrix,
                                                        int screenWidth,
                                                        int screenHeight,
                                                        rapidjson::Document &particleVolumePropertiesJson) {
    /* TODO make a GUI and make these modifiable by the GUI */
    /* each particle system should have a json properties file */
    glm::vec3 Center (0), orientation(0);
    float R = 10.0f, r = 1.0f, H = 100.0f;
    float initialScale = 5.0f, velocityMag = 150.0f;
    float diffusionFactor = 50.0f, buoyancyFactor = 50.0f;
    int delayMs = 20, liveForInMs = 3000;
    int numParticles = 100;
    bool result = true;
    m_blendType = ALPHA_BLENDING;

    if (particleVolumePropertiesJson.HasParseError()) {
        assert (false);
    }

    if (particleVolumePropertiesJson.HasMember ("center")) {
        Center.x = particleVolumePropertiesJson["center"].GetArray()[0].GetFloat();
        Center.y = particleVolumePropertiesJson["center"].GetArray()[1].GetFloat();
        Center.z = particleVolumePropertiesJson["center"].GetArray()[2].GetFloat();
    }

    if (particleVolumePropertiesJson.HasMember ("orientation")) {
        orientation.x = glm::radians(particleVolumePropertiesJson["orientation"].GetArray()[0].GetFloat());
        orientation.y = glm::radians(particleVolumePropertiesJson["orientation"].GetArray()[1].GetFloat());
        orientation.z = glm::radians(particleVolumePropertiesJson["orientation"].GetArray()[2].GetFloat());
    }

    if (particleVolumePropertiesJson.HasMember ("blendType")) {
        if (strcmp (particleVolumePropertiesJson["blendType"].GetString(), "additive") == 0) {
            m_blendType = ADDITIVE_BLENDING;
        } else if (strcmp (particleVolumePropertiesJson["blendType"].GetString(), "alpha") == 0) {
            m_blendType = ALPHA_BLENDING;
        }
    }

    R = particleVolumePropertiesJson.HasMember("R") ?
    particleVolumePropertiesJson["R"].GetFloat() : R;

    r = particleVolumePropertiesJson.HasMember("r") ?
    particleVolumePropertiesJson["r"].GetFloat() : r;

    H = particleVolumePropertiesJson.HasMember("H") ?
    particleVolumePropertiesJson["H"].GetFloat() : H;

    initialScale = particleVolumePropertiesJson.HasMember("initialScale") ?
    particleVolumePropertiesJson["initialScale"].GetFloat() : initialScale;

    velocityMag = particleVolumePropertiesJson.HasMember("velocityMag") ?
    particleVolumePropertiesJson["velocityMag"].GetFloat() : velocityMag;

    diffusionFactor = particleVolumePropertiesJson.HasMember("diffusionFactor") ?
    particleVolumePropertiesJson["diffusionFactor"].GetFloat() : diffusionFactor;

    buoyancyFactor = particleVolumePropertiesJson.HasMember("buoyancyFactor") ?
    particleVolumePropertiesJson["buoyancyFactor"].GetFloat() : buoyancyFactor;

    delayMs = particleVolumePropertiesJson.HasMember("delayMs") ?
    particleVolumePropertiesJson["delayMs"].GetInt() : delayMs;

    liveForInMs = particleVolumePropertiesJson.HasMember("liveForInMs") ?
    particleVolumePropertiesJson["liveForInMs"].GetInt() : liveForInMs;

    numParticles = particleVolumePropertiesJson.HasMember("maxParticles") ?
    particleVolumePropertiesJson["maxParticles"].GetInt() : numParticles;

    P::liveForInMs = liveForInMs;

    glm::vec3 center = glm::vec3 (0, H, 0);
    glm::quat rot(orientation);
    for (int i = 0; i < numParticles; i++) {
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

        glm::vec3 dir = glm::vec3 (x, 0, z);
        dir *= (r / R);
        glm::vec3 tmp = center + dir;
        glm::vec3 velocity =  glm::normalize (tmp - glm::vec3 (x, 0, z)) * velocityMag;

        glm::vec3 rotatedPos = (rot * glm::vec3 (x, 0, z)) + Center;

        P p (rotatedPos, rot * velocity, glm::vec3 (initialScale), diffusionFactor, buoyancyFactor);
        p.msDelay = delayMs * i;
        p.msDelayCopy = p.msDelay;

        particles.push_back (std::move (p));
    }

    assert (particles.size () == numParticles);

    createVao();
    matricesBuffer = new float[numParticles * 5];

    result &= getRenderingShader().updateUniform ("particleSampler", 0);
    result &= getRenderingShader().updateUniform ("depthSampler", 3);
    result &= getRenderingShader().updateUniform ("projectionMatrix", (void *) &projectionMatrix);
    result &= getRenderingShader().updateUniform ("liveForInMs", (void *) &P::getLiveForInMs ());
    result &= getRenderingShader().updateUniform ("totalNumOfSubTxts", P::getTexture().getTotalNumOfSubTxts ());
    result &= getRenderingShader().updateUniform ("numOfSubTxtsH", P::getTexture().getNumOfSubTxtsH ());
    result &= getRenderingShader().updateUniform ("numOfSubTxtsW", P::getTexture().getNumOfSubTxtsW ());
    result &= getRenderingShader().updateUniform ("subWidth", P::getTexture().getSubWidth());
    result &= getRenderingShader().updateUniform ("subHeight", P::getTexture().getSubHeight());
    result &= getRenderingShader().updateUniform ("screenHeight", (void *) &screenHeight);
    result &= getRenderingShader().updateUniform ("screenWidth", (void *) &screenWidth);

    assert (result);
}

template <typename P>
void ParticleRenderer<P>::update (Camera &camera, double dt) {
    const glm::mat4 viewMatrix = camera.getViewMatrix ();
    const glm::vec3 cameraPosition = camera.getPosition ();

    bool result = getRenderingShader().updateUniform ("viewMatrix", (void *) &viewMatrix);
    assert (result);

    for (P &particle : particles) {
        /* TODO remove this hardcode */
        particle.update (dt, camera);
    }

    std::sort (std::begin (particles),
               std::end (particles),
               [&] (P &p1, P &p2) {
                    return (p1.getDistanceToCamera () < p2.getDistanceToCamera ());
                });

    /* TODO create here thread for sorting after X configurable updates */
}

template <typename P>
void ParticleRenderer<P>::draw () {
    int offset = 0;

    glEnable (GL_BLEND);
    if (m_blendType == ADDITIVE_BLENDING) {
        glBlendFunc (GL_ONE, GL_ONE);
    } else if (m_blendType == ALPHA_BLENDING) {
        glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glDepthMask (GL_FALSE);

    /* TODO make sure here the particles are sorted (wait for the thread in update) */

    for (unsigned int i = 0; i < particles.size (); i++) {
        insertParticleAttributesInBuffer (particles[i], offset);
    }
    updateVbo ();

    glBindVertexArray (vaoHandle);
    getRenderingShader().bind ();
    P::getTexture().use();
    glDrawArraysInstanced(GL_POINTS, 0, 1, particles.size());
    glBindVertexArray(0);

    glDisable (GL_BLEND);
    glDepthMask (GL_TRUE);
}

template <typename P>
void ParticleRenderer<P>::createVao(){
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

template <typename P>
void ParticleRenderer<P>::createEmptyVbo(){
    glGenBuffers(1, &vboHandle);
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferData(GL_ARRAY_BUFFER, (NUM_OF_BYTES_PER_INSTANCE + sizeof (float)) * particles.size(), 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename P>
void ParticleRenderer<P>::insertParticleAttributesInBuffer(P &particle, int &offset) {
    const glm::vec3 &pos = particle.getInstaPosition ();
    const glm::vec3 &scale = particle.getScale ();

    matricesBuffer[offset++] = pos.x;
    matricesBuffer[offset++] = pos.y;
    matricesBuffer[offset++] = pos.z;
    matricesBuffer[offset++] = scale.x;
    matricesBuffer[offset++] = particle.getAliveForInMs ();
}

template <typename P>
void ParticleRenderer<P>::updateVbo(){
    glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (NUM_OF_BYTES_PER_INSTANCE + sizeof (float)) * particles.size(), (void *) matricesBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template <typename P>
ParticleRenderer<P>::~ParticleRenderer()
{
    glDeleteBuffers(1, &vboHandle);
    glDeleteVertexArrays(1, &vaoHandle);

    delete[] matricesBuffer;
}

template <typename P>
Shader &ParticleRenderer<P>::getRenderingShader()
{
    static Shader shader ("res/shaders/particleRenderShader.json");

    return shader;
}

#endif // PARTICLERENDERER_H
