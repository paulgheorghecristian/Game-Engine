#include "WaterRenderer.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "Entity.h"
#include "Mesh.h"
#include "RenderComponent.h"
#include "InstanceRenderComponent.h"
#include "RenderingMaster.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "ParticleEmitter.h"
#include "DirectionalLight.h"
#include "BlackHole.h"

static float jsonFloat(const rapidjson::Value &json, const char *key, float def) {
    return json.HasMember(key) && json[key].IsNumber() ? json[key].GetFloat() : def;
}

static glm::vec3 jsonVec3(const rapidjson::Value &json, const char *key, const glm::vec3 &def) {
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() < 3) return def;
    return glm::vec3(json[key][0].GetFloat(), json[key][1].GetFloat(), json[key][2].GetFloat());
}

static glm::vec2 jsonVec2(const rapidjson::Value &json, const char *key, const glm::vec2 &def) {
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() < 2) return def;
    return glm::vec2(json[key][0].GetFloat(), json[key][1].GetFloat());
}

WaterRenderer::WaterRenderer(const rapidjson::Value &json, int renderWidth, int renderHeight) :
    m_height(jsonFloat(json, "height", 0.0f)),
    m_center(jsonVec2(json, "center", glm::vec2(0))),
    m_size(jsonVec2(json, "size", glm::vec2(1000))),
    m_tiling(jsonFloat(json, "tiling", 120.0f)),
    m_waveStrength(jsonFloat(json, "waveStrength", 0.02f)),
    m_waveSpeed(jsonFloat(json, "waveSpeed", 0.03f)),
    m_waveHeight(jsonFloat(json, "waveHeight", 0.6f)),
    m_shallowColor(jsonVec3(json, "shallowColor", glm::vec3(0.75f, 0.9f, 0.95f))),
    m_deepColor(jsonVec3(json, "deepColor", glm::vec3(0.0f, 0.04f, 0.08f))),
    m_murkDepth(jsonFloat(json, "murkDepth", 60.0f)),
    m_softEdge(jsonFloat(json, "softEdge", 4.0f)),
    m_reflectivity(jsonFloat(json, "reflectivity", 1.0f)),
    m_reflectionGlow(jsonFloat(json, "reflectionGlow", 1.0f)),
    m_reflectionValid(false),
    m_width(renderWidth), m_height_px(renderHeight),
    /* floating point: reflections keep bright lights above 1 (bloomed later,
     * like the scene), and the refraction copy is taken from the HDR image */
    m_reflectionFB(std::max(16, renderWidth / 2), std::max(16, renderHeight / 2), 1, false, GL_RGBA16F),
    m_sceneCopyFB(renderWidth, renderHeight, 1, false, GL_RGBA16F),
    m_reflectionParticlesFB(std::max(16, renderWidth / 2), std::max(16, renderHeight / 2), 1, false, GL_RGBA16F),
    m_reflectionTexture(m_reflectionFB.getRenderTargets()[0], 0),
    m_sceneCopyTexture(m_sceneCopyFB.getRenderTargets()[0], 1),
    m_reflectionParticlesTexture(m_reflectionParticlesFB.getRenderTargets()[0], 8),
    m_dudvTexture("res/textures/waterDUDV.bmp", 2),
    m_normalTexture("res/textures/waterNormalMap.bmp", 3),
    m_waterShader("res/shaders/water.json"),
    m_reflectionShader("res/shaders/waterReflection.json")
{
    /* a grid (not a single quad) so the vertex shader can add gentle swell */
    m_mesh = Mesh::getSurface(96, 96);
    m_modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(m_center.x, m_height, m_center.y));
    m_modelMatrix = glm::scale(m_modelMatrix, glm::vec3(m_size.x * 0.5f, 1.0f, m_size.y * 0.5f));

    glm::vec2 screenSize(renderWidth, renderHeight);
    m_waterShader.updateUniform("modelMatrix", (void *) &m_modelMatrix);
    m_waterShader.updateUniform("screenSize", (void *) &screenSize);
    m_waterShader.updateUniform("reflectionSampler", 0);
    m_waterShader.updateUniform("refractionSampler", 1);
    m_waterShader.updateUniform("dudvSampler", 2);
    m_waterShader.updateUniform("normalMapSampler", 3);
    m_waterShader.updateUniform("depthSampler", 4);
    m_waterShader.updateUniform("particlesSampler", 5);
    m_waterShader.updateUniform("volumetricLightSampler", 6);
    m_waterShader.updateUniform("flareSampler", 7);
    m_waterShader.updateUniform("reflectionParticlesSampler", 8);
    m_waterShader.updateUniform("tiling", m_tiling);
    m_waterShader.updateUniform("waveStrength", m_waveStrength);
    m_waterShader.updateUniform("waveSpeed", m_waveSpeed);
    m_waterShader.updateUniform("waveHeight", m_waveHeight);
    m_waterShader.updateUniform("shallowColor", (void *) &m_shallowColor);
    m_waterShader.updateUniform("deepColor", (void *) &m_deepColor);
    m_waterShader.updateUniform("murkDepth", m_murkDepth);
    m_waterShader.updateUniform("softEdge", m_softEdge);
    m_waterShader.updateUniform("reflectivity", m_reflectivity);

    float clipHeight = m_height - 1.0f;
    m_reflectionShader.updateUniform("textureSampler", 0);
    m_reflectionShader.updateUniform("clipHeight", clipHeight);
}

WaterRenderer::~WaterRenderer()
{
    if (m_farDepthTexture != 0) {
        glDeleteTextures(1, &m_farDepthTexture);
    }
    delete m_mesh;
}

void WaterRenderer::uploadForwardLights(Shader &shader, const glm::vec3 &referencePoint)
{
    struct ForwardLight {
        glm::vec3 position, color;
        glm::vec4 spot; /* xyz: direction, w: cos(cone half angle); w < -1 for point lights */
        float radius, distance;
    };
    std::vector<ForwardLight> candidates;

    for (Light *light : RenderingMaster::getInstance()->getLights()) {
        ForwardLight fl;
        const Transform &t = light->getTransform();
        fl.position = t.getPosition();
        fl.color = light->getLightColor();
        if (dynamic_cast<PointLight *>(light) != NULL) {
            fl.radius = t.getScale().x * 0.75f; /* same cut-off as the deferred pass */
            fl.spot = glm::vec4(0, 0, 0, -2.0f);
        } else if (dynamic_cast<SpotLight *>(light) != NULL) {
            fl.radius = t.getScale().z;
            glm::vec3 dir = glm::normalize(glm::mat3(t.getModelMatrix()) * glm::vec3(0, 0, -1));
            fl.spot = glm::vec4(dir, glm::cos(dynamic_cast<SpotLight *>(light)->getConeAngle()));
        } else {
            continue; /* directional lights come in as the moon */
        }
        fl.distance = glm::length(fl.position - referencePoint) - fl.radius;
        candidates.push_back(fl);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const ForwardLight &a, const ForwardLight &b) { return a.distance < b.distance; });
    int count = std::min((int) candidates.size(), MAX_FORWARD_LIGHTS);

    glm::vec3 positions[MAX_FORWARD_LIGHTS], colors[MAX_FORWARD_LIGHTS];
    glm::vec4 spots[MAX_FORWARD_LIGHTS];
    float radii[MAX_FORWARD_LIGHTS];
    for (int i = 0; i < count; i++) {
        positions[i] = candidates[i].position;
        colors[i] = candidates[i].color;
        spots[i] = candidates[i].spot;
        radii[i] = candidates[i].radius;
    }

    shader.updateUniform("numLights", count);
    GLuint program = shader.getShaderId();
    GLint loc;
    shader.bind();
    if ((loc = glGetUniformLocation(program, "lightPositions")) >= 0 && count > 0)
        glUniform3fv(loc, count, &positions[0].x);
    if ((loc = glGetUniformLocation(program, "lightColors")) >= 0 && count > 0)
        glUniform3fv(loc, count, &colors[0].x);
    if ((loc = glGetUniformLocation(program, "lightSpots")) >= 0 && count > 0)
        glUniform4fv(loc, count, &spots[0].x);
    if ((loc = glGetUniformLocation(program, "lightRadii")) >= 0 && count > 0)
        glUniform1fv(loc, count, radii);
}

static void uploadEnvironment(Shader &shader)
{
    const Environment &env = RenderingMaster::getInstance()->getEnvironment();
    float ambient = env.ambient;
    shader.updateUniform("moonDirection", (void *) &env.lightDirection);
    shader.updateUniform("moonColor", (void *) &env.lightColor);
    shader.updateUniform("fogColor", (void *) &env.fogColor);
    shader.updateUniform("fogDensity", env.fogDensity);
    shader.updateUniform("fogHeightFalloff", env.fogHeightFalloff);
    shader.updateUniform("fogBaseHeight", env.fogBaseHeight);
    shader.updateUniform("fogMoonGlow", env.fogMoonGlow);
    RenderingMaster::getInstance()->uploadFogCutoff(shader);
    shader.updateUniform("ambient", ambient);
    shader.updateUniform("ambientSky", (void *) &env.ambientSky);
    shader.updateUniform("ambientGround", (void *) &env.ambientGround);
    shader.updateUniform("emissiveStrength", env.emissiveStrength);

    /* forward-lit passes use the scene's directional light (moonlight) */
    glm::vec3 sunColor(0.0f), sunDirection = env.moonDirection;
    for (Light *light : RenderingMaster::getInstance()->getLights()) {
        if (DirectionalLight *dir = dynamic_cast<DirectionalLight *>(light)) {
            sunColor = dir->getLightColor();
            sunDirection = glm::normalize(dir->getLightDirection());
            break;
        }
    }
    shader.updateUniform("sunColor", (void *) &sunColor);
    shader.updateUniform("sunDirection", (void *) &sunDirection);
}

void WaterRenderer::renderReflection(const std::vector<Entity *> &entities)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    Camera *camera = rm->getCamera();

    /* from below the surface there is nothing to mirror */
    m_reflectionValid = camera->getPosition().y > m_height;
    if (!m_reflectionValid) {
        return;
    }

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 mirror = glm::translate(glm::mat4(1.0f), glm::vec3(0, m_height, 0)) *
                       glm::scale(glm::mat4(1.0f), glm::vec3(1, -1, 1)) *
                       glm::translate(glm::mat4(1.0f), glm::vec3(0, -m_height, 0));
    glm::mat4 mirroredView = view * mirror;
    glm::vec3 mirroredCamera = glm::vec3(mirror * glm::vec4(camera->getPosition(), 1.0f));

    m_reflectionFB.bindSingleRenderTarget(0);
    /* mirroring flips triangle winding */
    glFrontFace(GL_CW);

    rm->skyShader->updateUniform("viewMatrix", (void *) &mirroredView);
    rm->drawSky();
    rm->skyShader->updateUniform("viewMatrix", (void *) &view);
    /* the black holes in the sky it mirrors (the scene then covers them) */
    rm->renderBlackHolesMirrored(m_reflectionFB, mirroredView, mirroredCamera);

    Shader &shader = m_reflectionShader;
    shader.updateUniform("viewMatrix", (void *) &mirroredView);
    shader.updateUniform("projectionMatrix", (void *) &rm->getProjectionMatrix());
    shader.updateUniform("cameraPosition", (void *) &mirroredCamera);
    uploadEnvironment(shader);
    uploadForwardLights(shader, camera->getPosition());
    rm->uploadWind(shader);

    glEnable(GL_CLIP_DISTANCE0);
    for (Entity *entity : entities) {
        if (entity->getComponent(Entity::Flags::BILL) != NULL) {
            continue;
        }
        RenderComponent *rc = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
        if (rc != NULL && !rc->isBeyondDrawDistance(entity->getTransform())) {
            shader.updateUniform("instanced", false);
            rc->render(&shader);
        }
        InstanceRenderComponent *irc = (InstanceRenderComponent *) entity->getComponent(Entity::Flags::INSTANCE_RENDERABLE);
        if (irc != NULL) {
            shader.updateUniform("instanced", true);
            irc->render(&shader);
        }
    }
    glDisable(GL_CLIP_DISTANCE0);
    glFrontFace(GL_CCW);

    m_reflectionFB.unbind();

    /* particles too (light halos, flames, sparks, lanterns), mirrored, into
     * their own buffer that shares the reflection's depth (so reflected
     * geometry still hides them) */
    m_reflectionParticlesFB.bindSingleRenderTarget(0);
    if (!rm->particleEmitters.empty()) {
        int w = m_reflectionFB.getWidth(), h = m_reflectionFB.getHeight();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_reflectionFB.getFrameBufferObject());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_reflectionParticlesFB.getFrameBufferObject());
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, m_reflectionParticlesFB.getFrameBufferObject());
        if (m_farDepthTexture == 0) {
            float one = 1.0f;
            glGenTextures(1, &m_farDepthTexture);
            glBindTexture(GL_TEXTURE_2D, m_farDepthTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &one);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_farDepthTexture);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);
        rm->uploadFogCutoff(ParticleEmitter::getShader());
        for (ParticleEmitter *emitter : rm->particleEmitters) {
            emitter->draw(rm->getProjectionMatrix(), mirroredView, m_width, m_height_px, rm->getRenderAlpha());
        }
        glEnable(GL_CULL_FACE);
    }
    m_reflectionParticlesFB.unbind();

    /* blurred levels for the reflection glow */
    GLuint reflectionTexture = m_reflectionFB.getRenderTargets()[0];
    glBindTexture(GL_TEXTURE_2D, reflectionTexture);
    if (!m_reflectionMipmapsSet) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        m_reflectionMipmapsSet = true;
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void WaterRenderer::render(FrameBuffer &target)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    Camera *camera = rm->getCamera();

    /* refraction = the lit scene behind the surface */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFrameBufferObject());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_sceneCopyFB.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, m_width, m_height_px, 0, 0, m_width, m_height_px,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, m_width, m_height_px);

    /* depth is tested manually against the G-buffer depth in the shader */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    Shader &shader = m_waterShader;
    float time = rm->getTime();
    int reflectionValid = m_reflectionValid ? 1 : 0;
    shader.updateUniform("viewMatrix", (void *) &camera->getViewMatrix());
    shader.updateUniform("projectionMatrix", (void *) &rm->getProjectionMatrix());
    shader.updateUniform("cameraPosition", (void *) &camera->getPosition());
    shader.updateUniform("time", time);
    shader.updateUniform("hasReflection", reflectionValid);
    shader.updateUniform("reflectionGlow", m_reflectionGlow);
    float waveHeight = m_waveHeight * (1.0f + 5.0f * rm->getStorm());   /* the aftermath's storm swell */
    shader.updateUniform("waveHeight", waveHeight);
    uploadEnvironment(shader);
    uploadForwardLights(shader, camera->getPosition());
    rm->uploadWind(shader);
    /* a black hole in the sky: its disk's light glitters on the waves, a
     * broken path across the water towards it (like the moon's) */
    glm::vec3 holeDirection(0.0f, 1.0f, 0.0f), holeColor(0.0f);
    for (BlackHole *hole : rm->blackHoles) {
        if (hole->sky && !hole->kugelblitz) {
            holeDirection = glm::normalize(hole->direction);
            holeColor = hole->diskColor * (hole->diskBrightness * hole->diskOpacity * 0.12f) *
                        glm::smoothstep(-0.02f, 0.15f, holeDirection.y);
            break;
        }
    }
    shader.updateUniform("holeGlintDirection", (void *) &holeDirection);
    shader.updateUniform("holeGlintColor", (void *) &holeColor);

    m_reflectionTexture.use(0);
    m_sceneCopyTexture.use(1);
    m_dudvTexture.use(2);
    m_normalTexture.use(3);
    rm->depthTexture->use(4);
    rm->particlesRTTexture.use(5);
    rm->getVolumetricLightTexture().use(6);
    rm->getFlareTexture().use(7);
    m_reflectionParticlesTexture.use(8);

    shader.bind();
    m_mesh->draw();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}
