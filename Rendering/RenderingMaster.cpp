#include "RenderingMaster.h"
#include "ProfilingTimer.h"

#include "ParticleRenderer.h"
#include "ParticleFactory.h"
#include "GUI.h"
#include "MathUtils.h"

#include "SpotLight.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "Common.h"
#include "PhysicsMaster.h"
#include "RenderingObject.hpp"
#include "Button.hpp"
#include "Font.h"
#include "ParticleEmitter.h"
#include "GrassRenderer.h"
#include "BlackHole.h"
#include "LensSurroundings.h"
#include "Material.h"
#include "WaterRenderer.h"

#include <typeinfo>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"


RenderingMaster *RenderingMaster::m_instance = NULL;

/* Volumetric light is ray-marched at (render size / divisor). 2 keeps beam
 * edges clean where geometry cuts through them; 4 is the old, cheaper setting.
 * Override with GE_VOLUMETRIC_DIV. */
static int volumetricDivisor() {
    static int divisor = -1;
    if (divisor < 0) {
        const char *env = std::getenv("GE_VOLUMETRIC_DIV");
        divisor = env ? std::max(1, std::atoi(env)) : 2;
    }
    return divisor;
}
Shader RenderingMaster::simpleTextShader;
glm::mat4 RenderingMaster::orthoProjectionMatrix;

RenderingMaster::RenderingMaster(Display *display,
                                 Camera *camera,
                                 glm::mat4 projectionMatrix,
                                 int rw, int rh) :  renderWidth(rw), renderHeight(rh),
                                                                particleForwardRenderFramebuffer (rw/2, rh/2, 1),
                                                                particlesRTTexture (particleForwardRenderFramebuffer.getRenderTargets ()[0], 6),
                                                                volumetricLightFB (rw/volumetricDivisor(), rh/volumetricDivisor(), 1),
                                                                volumetricLightTxt (volumetricLightFB.getRenderTargets()[0], 9),
                                                                spotLightFlares(rw, rh, 1),
                                                                spotLightFlaresTxt(spotLightFlares.getRenderTargets()[0], 10),
                                                                fxaaFrameBuffer(rw, rh, 1)
{
    int albedoTextureUnit = 0, normalTextureUnit = 1, lightAccumulationTextureUnit = 2;
    int depthTextureUnit = 3, outputType = 1, blurredLightAccUnit = 4, spotLightDepthMapUnit = 5;
    bool result = true;

    this->display = display;
    this->camera = camera;
    this->projectionMatrix = projectionMatrix;
    /* far plane of the projection we were given (restored when a map turns the fog cut-off off) */
    m_defaultFarPlane = m_farPlane = projectionMatrix[3][2] / (projectionMatrix[2][2] + 1.0f);

    updateDt = display->getFrameTimeInMs();

    gBuffer.generate (renderWidth, renderHeight);

    deferredShading_SceneShader.construct ("res/shaders/deferredShadingSceneShader.json");
    result &= deferredShading_SceneShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    deferredShading_InstanceRender.construct ("res/shaders/deferredShadingInstanceRender.json");
    result &= deferredShading_InstanceRender.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    deferredShading_AnimRender.construct("res/shaders/deferredShadingAnimSceneShader.json");
    result &= deferredShading_AnimRender.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    deferredShading_BufferCombinationShader.construct ("res/shaders/bufferCombinationShader.json");
    result &= deferredShading_BufferCombinationShader.updateUniform("normalSampler", (void *) &normalTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("depthSampler", (void *) &depthTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("lightAccumulationSampler", (void *) &lightAccumulationTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("colorSampler", (void *) &albedoTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("blurredLightAccSampler", (void *) &blurredLightAccUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("outputType", (void *) &outputType);
    result &= deferredShading_BufferCombinationShader.updateUniform("spotLightDepthMap", (void *) &spotLightDepthMapUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("particlesSampler", 6);
    result &= deferredShading_BufferCombinationShader.updateUniform("dirLightDepthSampler", 8);
    result &= deferredShading_BufferCombinationShader.updateUniform("volumetricLightSampler", 9);
    result &= deferredShading_BufferCombinationShader.updateUniform("flareSampler", 10);
    result &= deferredShading_BufferCombinationShader.updateUniform("flareBlurSampler", 11);
    result &= deferredShading_BufferCombinationShader.updateUniform("roughnessSampler", 12);
    assert (result);

    result &= SpotLight::getLightAccumulationShader().updateUniform("spotLightDepthSampler", 0);
    result &= SpotLight::getLightAccumulationShader().updateUniform("eyeSpaceNormalSampler", 1);
    result &= SpotLight::getLightAccumulationShader().updateUniform("depthSampler", 2);
    result &= SpotLight::getLightAccumulationShader().updateUniform("screenWidth", renderWidth);
    result &= SpotLight::getLightAccumulationShader().updateUniform("screenHeight", renderHeight);
    result &= SpotLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= SpotLight::getLightAccumulationShader().updateUniform("roughnessSampler", 3);
    result &= SpotLight::getLightAccumulationShader().updateUniform("blueNoiseSampler", 4);
    assert (result);

    result &= DirectionalLight::getLightAccumulationShader().updateUniform("dirLightDepthSampler", 0);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("eyeSpaceNormalSampler", 1);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("depthSampler", 2);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("screenWidth", renderWidth);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("screenHeight", renderHeight);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("roughnessSampler", 3);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("blueNoiseSampler", 4);
    assert(result);

    result &= PointLight::getLightAccumulationShader().updateUniform("eyeSpaceNormalSampler", 1);
    result &= PointLight::getLightAccumulationShader().updateUniform("depthSampler", 2);
    result &= PointLight::getLightAccumulationShader().updateUniform("screenWidth", renderWidth);
    result &= PointLight::getLightAccumulationShader().updateUniform("screenHeight", renderHeight);
    result &= PointLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= PointLight::getLightAccumulationShader().updateUniform("roughnessSampler", 3);
    assert(result);

    deferredShading_StencilBufferCreator.construct ("res/shaders/stencilBufferCreator.json");
    result &= deferredShading_StencilBufferCreator.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    depthMapCreator.construct("res/shaders/depthMapCreator.json");
    depthMapCreatorInstanceRender.construct("res/shaders/depthMapCreatorInstanceRender.json");

    volumetricLightShader.construct("res/shaders/volumetricLight.json");
    result &= volumetricLightShader.updateUniform("volumetricLightDepthSampler", 0);
    result &= volumetricLightShader.updateUniform("depthSampler", 1);
    result &= volumetricLightShader.updateUniform("blueNoiseSampler", 2);
    result &= volumetricLightShader.updateUniform("screenWidth", renderWidth);
    result &= volumetricLightShader.updateUniform("screenHeight", renderHeight);
    volumetricLightShader.updateUniform("volumetricDivisor", volumetricDivisor());
    result &= volumetricLightShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    flareShader.construct("res/shaders/FlareShader.json");
    result &= flareShader.updateUniform("depthSampler", 0);
    result &= flareShader.updateUniform("screenWidth", renderWidth);
    result &= flareShader.updateUniform("screenHeight", renderHeight);
    result &= flareShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    lastStageShader.construct("res/shaders/finalStage.json");
    result &= deferredShading_BufferCombinationShader.updateUniform("colorSampler", 0);
    assert(result);

    DebugRenderer::lineShader.construct("res/shaders/lineShader.json");
    result &= DebugRenderer::lineShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    albedoTexture = new Texture (gBuffer.getColorTexture(), albedoTextureUnit);
    normalTexture = new Texture (gBuffer.getNormalTexture(), normalTextureUnit);
    lightAccumulationTexture = new Texture (gBuffer.getLightAccumulationTexture(), lightAccumulationTextureUnit);
    depthTexture = new Texture (gBuffer.getDepthTexture(), depthTextureUnit);
    roughnessTexture = new Texture(gBuffer.getRoughnessTexture(), 12);

    hBlurPostProcess = new PostProcess (renderWidth/4, renderHeight/4,
                                        spotLightFlaresTxt.getTextureId(),
                                        "res/shaders/hBlurPostProcess.json");

    wBlurPostProcess = new PostProcess (renderWidth/4, renderHeight/4,
                                        hBlurPostProcess->getResultingTextureId(),
                                        "res/shaders/wBlurPostProcess.json");

    flarePostProcess = new PostProcess(renderWidth/4, renderHeight/4,
                                        wBlurPostProcess->getResultingTextureId(),
                                        "res/shaders/FlarePostProcess.json");
    flarePostProcess->getShader().updateUniform("lensFlareColorSampler", 1);
    lensFlareColorTxt = new Texture("res/textures/lenscolor.bmp", 1);
    blueNoiseTexture = new Texture("res/textures/BlueNoise64Tiled.png", 2);
    /* NASA Lunar Reconnaissance Orbiter color map (CGI Moon Kit, NASA Scientific
     * Visualization Studio, public domain), equirectangular, 0 longitude in the middle */
    m_moonTexture = new Texture("res/textures/moon_lroc.jpg", 15);

    fxaaPostProcess = new PostProcess(renderWidth, renderHeight,
                                        fxaaFrameBuffer.getRenderTargets()[0],
                                        "res/shaders/fxaa.json");

    screenSizeRectangle = Mesh::getRectangle();

    GUI::init (display->getWidth(), display->getHeight());
    simpleTextShader.construct ("res/shaders/simpleTextShader.json");
    result &= simpleTextShader.updateUniform ("projectionMatrix", (void *) &GUI::projectionMatrix);
    result &= simpleTextShader.updateUniform ("fontAtlas", 0);
    assert (result);

    for (int i = 0; i < GUIVarsEnum_f::NUM_VARS_f; i++) {
        data_f[i] = 0;
    }

    for (int i = 0; i < GUIVarsEnum_vec3::NUM_VARS_v3; i++) {
        data_vec3[i] = {0,0,0};
    }

    for (int i = 0; i < GUIVarsEnum_int::NUM_VARS_i; i++) {
        data_i[i] = 0;
    }

    SpotLight::setLightMesh(RenderingObject::loadObject("res/models/SpotLightMesh5.obj"));
    PointLight::setLightMesh(RenderingObject::loadObject("res/models/lightsphere.obj"));

    skyShader = new Shader("res/shaders/skyShader.json");
    skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);

    orthoProjectionMatrix = glm::ortho(0.0f, (float) display->getWidth(), 0.0f, (float) display->getHeight(), 1.0f, 1000.0f);
    Button::getButtonShader().updateUniform("projectionMatrix", (void *) &orthoProjectionMatrix);

    smokeRenderer = NULL;
    SpotLight::getLightAccumulationShader().updateUniform("albedoSampler", 6);
    PointLight::getLightAccumulationShader().updateUniform("albedoSampler", 6);
    DirectionalLight::getLightAccumulationShader().updateUniform("albedoSampler", 6);
    setEnvironment(Environment());
    glm::mat4 inverseProjection = glm::inverse(projectionMatrix);
    deferredShading_BufferCombinationShader.updateUniform("inverseProjectionMatrix", (void *) &inverseProjection);

    Font::addFontToCache("res/fonts/normalFont");
    Font::addFontToCache("res/fonts/horror");
    Font::addFontToCache("res/fonts/horror3");
}

RenderingMaster::~RenderingMaster()
{
    delete camera;

    delete albedoTexture;
    delete normalTexture;
    delete lightAccumulationTexture;
    delete depthTexture;
    delete roughnessTexture;

    delete screenSizeRectangle;

    delete hBlurPostProcess;
    delete wBlurPostProcess;
    delete flarePostProcess;
    delete smokeRenderer;
    delete m_sunDisc;
    if (m_skyCube != 0) glDeleteTextures(1, &m_skyCube);
    if (m_skyCubeFbo != 0) glDeleteFramebuffers(1, &m_skyCubeFbo);
    delete m_ssaoFB; delete m_ssaoBlurFB; delete m_hdrFB;
    delete m_bloomHalfA; delete m_bloomHalfB; delete m_bloomQuarterA; delete m_bloomQuarterB;
    delete m_ssaoShader; delete m_ssaoBlurShader; delete m_bloomBrightShader; delete m_blurShader; delete m_tonemapShader;
    delete water;
    delete grass;
    for (BlackHole *hole : blackHoles) delete hole;
    delete m_lensCopyFB;
    delete m_mirrorLensCopyFB;
    if (m_farDepthTexture != 0) glDeleteTextures(1, &m_farDepthTexture);
    delete m_lensSkyFB;
    delete m_lensSurroundings;
    for (ParticleEmitter *emitter : particleEmitters) {
        delete emitter;
    }

    display->close();
    delete display;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void RenderingMaster::init(Display *display,
                           Camera *camera,
                           glm::mat4 projectionMatrix,
                           float renderScale)
{
    if (m_instance == NULL) {
        renderScale = glm::clamp(renderScale, 0.25f, 1.0f);
        int rw = std::max(16, (int) (display->getWidth() * renderScale + 0.5f));
        int rh = std::max(16, (int) (display->getHeight() * renderScale + 0.5f));
        m_instance = new RenderingMaster (display, camera, projectionMatrix, rw, rh);
    }
}

RenderingMaster *RenderingMaster::getInstance()
{
    return m_instance;
}

void RenderingMaster::destroy()
{
    if (m_instance != NULL) {
        delete m_instance;
    }
}

void RenderingMaster::clearScreen(float r, float g, float b, float a, GLbitfield field)
{
    display->clear (r, g, b, a, field);
}

void RenderingMaster::swapBuffers()
{
    display->swapBuffers ();
}

void RenderingMaster::moveCameraForward(float distance)
{
    camera->moveForward(distance);
}

void RenderingMaster::moveCameraSideways(float distance)
{
    camera->moveSideways(distance);
}

void RenderingMaster::rotateCameraX(float rotX) {
    camera->rotateX(rotX);
}
void RenderingMaster::rotateCameraY(float rotY)
{
    camera->rotateY(rotY);
}

Display *RenderingMaster::getDisplay()
{
    return display;
}

Camera *RenderingMaster::getCamera()
{
    return camera;
}

const glm::mat4 &RenderingMaster::getProjectionMatrix()
{
    return projectionMatrix;
}

GBuffer &RenderingMaster::getGBuffer()
{
    return gBuffer;
}

void RenderingMaster::drawDeferredShadingBuffers()
{
    gBuffer.unbind();
    clearScreen(1, 1, 1, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (flaresNeeded()) {
        glm::vec2 sunUV(0.5f);
        int sunMode = (isSunFlareVisible() && sunScreenPosition(sunUV)) ? 1 : 0;
        flarePostProcess->getShader().updateUniform("sunMode", sunMode);
        flarePostProcess->getShader().updateUniform("sunScreenPosition", (void *) &sunUV);

        hBlurPostProcess->bind();
        hBlurPostProcess->process();

        wBlurPostProcess->bind();
        wBlurPostProcess->process();

        flarePostProcess->bind();
        flarePostProcess->process({lensFlareColorTxt});
    }

    const bool hdr = m_environment.hdr;
    const bool skyReflections = m_environment.pbr;
    if (skyReflections) {
        PT_FromHere("skyCube");
        renderSkyCube();
        PT_ToHere("skyCube");
    }
    float skyMaxLod = std::log2((float) SKY_CUBE_SIZE);
    deferredShading_BufferCombinationShader.updateUniform("skyReflections", skyReflections);
    deferredShading_BufferCombinationShader.updateUniform("skyCube", 14);
    deferredShading_BufferCombinationShader.updateUniform("skyCubeMaxLod", skyMaxLod);
    int aoUnit = 13;
    float aoStrength = 0.0f;
    if (m_environment.aoStrength > 0.0f) {
        PT_FromHere("ssao");
        renderSSAO();
        PT_ToHere("ssao");
        aoStrength = m_environment.aoStrength;
    }
    deferredShading_BufferCombinationShader.updateUniform("aoSampler", aoUnit);
    deferredShading_BufferCombinationShader.updateUniform("aoStrength", aoStrength);
    deferredShading_BufferCombinationShader.updateUniform("hdrEnabled", hdr);

    //START FXAA
    if (hdr) {
        if (m_hdrFB == nullptr) {
            m_hdrFB = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
        }
        /* the scene is composited in HDR, then bloomed and tone mapped into the FXAA input */
        m_hdrFB->bindSingleRenderTarget(0);
    } else {
        fxaaFrameBuffer.bindSingleRenderTarget(0);
    }
    deferredShading_BufferCombinationShader.bind();
    if (skyReflections) {
        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCube);
    }
    if (aoStrength > 0.0f) {
        glActiveTexture(GL_TEXTURE0 + aoUnit);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurFB->getRenderTargets()[0]);
    }

    albedoTexture->use();
    normalTexture->use();
    lightAccumulationTexture->use();
    depthTexture->use();
    roughnessTexture->use(12);
    particlesRTTexture.use ();
    /* debug view 8 shows the directional light's shadow map (point lights have none) */
    for (Light *light : lights) {
        if (dynamic_cast<DirectionalLight *>(light) != NULL) {
            light->getShadowMapTexture().use(8);
            break;
        }
    }
    volumetricLightTxt.use(9);
    flarePostProcess->getResultingTexture().use(10);
    wBlurPostProcess->getResultingTexture().use(11);
    /* with a black hole the lens flare goes on after it (in the tone mapping):
     * it is the camera's, so the hole must not bend or hide it */
    float flareInComposite = hdr && !blackHoles.empty() ? 0.0f : 1.0f;
    deferredShading_BufferCombinationShader.updateUniform("flareInComposite", flareInComposite);
    uploadGlowLights(deferredShading_BufferCombinationShader);
    /* reflections read the previous frame's finished HDR image (unit 15) */
    bool ssr = hdr && skyReflections && m_environment.reflections && m_prevFrameFB != nullptr;
    deferredShading_BufferCombinationShader.updateUniform("ssrEnabled", ssr);
    deferredShading_BufferCombinationShader.updateUniform("previousFrameSampler", 15);
    deferredShading_BufferCombinationShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    if (ssr) {
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, m_prevFrameFB->getRenderTargets()[0]);
        glActiveTexture(GL_TEXTURE0);
    }

    PT_FromHere("composite");
    screenSizeRectangle->draw();
    PT_ToHere("composite");

    deferredShading_BufferCombinationShader.unbind();
    if (hdr) {
        /* the water goes into the HDR image too, so its reflections and glints
         * are bloomed and tone mapped like everything else */
        PT_FromHere("water");
        if (water != NULL) {
            water->render(*m_hdrFB);
        }
        PT_ToHere("water");
        PT_FromHere("glass");
        renderGlass(*m_hdrFB);
        PT_ToHere("glass");
        PT_FromHere("blackHoles");
        renderBlackHoles(*m_hdrFB);
        PT_ToHere("blackHoles");
        /* keep this frame for the next one's reflections */
        if (m_environment.reflections && m_environment.pbr) {
            if (m_prevFrameFB == nullptr) {
                m_prevFrameFB = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
            }
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_hdrFB->getFrameBufferObject());
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_prevFrameFB->getFrameBufferObject());
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        m_hdrFB->unbind();
        PT_FromHere("bloomToneMap");
        renderBloomAndToneMap(); /* leaves fxaaFrameBuffer bound */
        PT_ToHere("bloomToneMap");
    } else {
        if (water != NULL) {
            water->render(fxaaFrameBuffer);
        }
        renderGlass(fxaaFrameBuffer);
        renderBlackHoles(fxaaFrameBuffer);
    }
    fxaaFrameBuffer.unbind();
    //END FXAA

    PT_FromHere("temporalAA");
    renderTemporalAA();
    PT_ToHere("temporalAA");

    /* FXAA writes the final image directly to the screen (it used to go to an
     * offscreen target followed by a full-screen copy pass) */
    PT_FromHere("fxaa");
    fxaaPostProcess->processToScreen(display->getWidth(), display->getHeight());
    PT_ToHere("fxaa");
}

void RenderingMaster::update() {
    if (!m_worldPaused) m_time += updateDt / 1000.0f;
    const float weather[5] = { m_wind, m_fog, m_clouds, m_calm, m_dust };
    if (m_baseEnvironment.dayLength <= 0.0f && !std::equal(weather, weather + 5, m_appliedWeather)) {
        applyEnvironment(m_baseEnvironment);   /* a fixed time of day still gets the storm */
    }

    if (m_baseEnvironment.dayLength > 0.0f) {
        float speed = m_timePaused ? 0.0f : m_timeScale;
        float t = m_environment.timeOfDay + (float) (updateDt / 1000.0) * speed / m_baseEnvironment.dayLength;
        applyEnvironment(Environment::atTimeOfDay(m_baseEnvironment, t));

        /* the scene's directional light follows the sun by day, the moon by night */
        for (Light *light : lights) {
            if (DirectionalLight *dir = dynamic_cast<DirectionalLight *>(light)) {
                if (!m_dirLightBaseCaptured) {
                    m_dirLightBaseColor = dir->getLightColor();
                    m_dirLightBaseCaptured = true;
                }
                const Environment &e = m_environment;
                bool sunUp = e.sunDirection.y > -0.02f;
                float moonUp = glm::smoothstep(-0.02f, 0.3f, e.moonDirection.y);
                dir->setLightDirection(e.lightDirection);
                dir->setLightColor(sunUp ? e.lightColor * 0.9f : m_dirLightBaseColor * moonUp);
                break;
            }
        }
    }
    Light::s_nightFactor = getNightFactor();
    Material::s_nightFactor = Light::s_nightFactor;
    ParticleEmitter::s_nightFactor = Light::s_nightFactor;

    if (smokeRenderer != NULL && !m_worldPaused) {
        smokeRenderer->update(*camera, updateDt);
    }
    for (ParticleEmitter *emitter : particleEmitters) {
        if (m_worldPaused) break;
        /* a beam emitter whose light was deleted in the editor */
        if (emitter->getLinkedLight() != nullptr &&
            std::find(lights.begin(), lights.end(), (Light *) emitter->getLinkedLight()) == lights.end()) {
            emitter->lightRemoved();
        }
        emitter->update(*camera, updateDt);
    }

    flarePostProcess->getShader().updateUniform("uGhostDispersal", data_f[0]);
    flarePostProcess->getShader().updateUniform("density", data_f[1]);
    flarePostProcess->getShader().updateUniform("exposure", data_f[2]);
    flarePostProcess->getShader().updateUniform("weight", data_f[3]);
    flarePostProcess->getShader().updateUniform("decay", data_f[4]);
    flarePostProcess->getShader().updateUniform("illuminationDecay", data_f[5]);
}

bool RenderingMaster::viewCulling()
{
    static const bool on = std::getenv("GE_NO_CULL") == NULL;
    return on;
}

void RenderingMaster::prepareFrame(float alpha) {
    m_frameNumber++;
    m_renderAlpha = glm::clamp(alpha, 0.0f, 1.0f);
    if (!m_worldPaused) m_renderTime = m_time - (1.0f - m_renderAlpha) * (float) (updateDt / 1000.0);

    /* temporal anti-aliasing: this frame's sub-pixel shift (Halton 2,3; the
     * projection's off-axis terms, which nothing else uses) */
    glm::vec2 jitter(0.0f);
    if (m_environment.temporalAA && m_environment.hdr) {
        auto halton = [](int i, int base) {
            float f = 1.0f, r = 0.0f;
            for (; i > 0; i /= base) { f /= base; r += f * (i % base); }
            return r;
        };
        m_taaFrame = (m_taaFrame + 1) % 8;
        jitter = glm::vec2((halton(m_taaFrame + 1, 2) - 0.5f) * 2.0f / renderWidth,
                           (halton(m_taaFrame + 1, 3) - 0.5f) * 2.0f / renderHeight);
    }
    if (projectionMatrix[2][0] != jitter.x || projectionMatrix[2][1] != jitter.y) {
        projectionMatrix[2][0] = jitter.x;
        projectionMatrix[2][1] = jitter.y;
        uploadProjection();
    }

    glm::vec3 forwardVectorInEyeSpace;
    glm::vec3 cameraForward = glm::normalize(camera->getForward());
    glm::mat4 cameraViewMatrix;

    cameraViewMatrix = camera->getViewMatrix ();
    forwardVectorInEyeSpace = glm::mat3 (cameraViewMatrix) * cameraForward;
    inverseViewMatrix = glm::inverse(cameraViewMatrix);
    skyShader->updateUniform("time", m_renderTime);
    /* clouds drift with time; the sky needs the camera to place them in the world */
    skyShader->updateUniform("cloudTime", m_renderTime);
    {
        /* a black hole in the sky: the moon stream in the sky shader spirals into it */
        BlackHole *skyHole = nullptr;
        for (BlackHole *hole : blackHoles) {
            if (hole->sky) { skyHole = hole; break; }
        }
        m_skyCloudCut = skyHole != nullptr && m_environment.cloudCoverage > 0.0f;
        /* a Kugelblitz sits in the moon, and eats it at its own pace */
        float collapse = -1.0f;
        if (skyHole != nullptr && skyHole->kugelblitz) {
            skyHole->direction = m_environment.moonDirection;
            float dt = m_lastCollapseTime < 0.0f ? 0.0f : glm::clamp(m_renderTime - m_lastCollapseTime, 0.0f, 0.1f);   /* hitches (loading) must not eat the countdown */
            dt *= m_kugelblitzTimeScale;   /* 0: paused (the timeline editor) */
            /* playback: from 0 it waits collapseDelay seconds, then ignites
             * and runs at collapseSpeed until the moon is gone */
            if (skyHole->playing) {
                bool waiting = false;
                if (skyHole->collapse <= 0.0f) {
                    skyHole->delayElapsed += dt;
                    waiting = skyHole->delayElapsed < skyHole->collapseDelay;
                }
                if (!waiting) {
                    skyHole->collapse = glm::clamp(skyHole->collapse + skyHole->collapseSpeed / 60.0f * dt, 0.0f, 1.0f);
                }
                if (skyHole->collapse >= 1.0f) {
                    skyHole->playing = false;
                }
            }
            collapse = skyHole->collapse;
            /* ignition: the moment collapse leaves 0 (running, or dragged) */
            const float IGNITION = 0.0005f;
            if (skyHole->previousCollapse >= 0.0f && skyHole->previousCollapse <= IGNITION && collapse > IGNITION) {
                skyHole->ignitionTime = m_renderTime;
            }
            /* the end, watched happening (a map loaded finished has no climax) */
            if (collapse < 1.0f) {
                skyHole->formedAt = -1.0f;
            } else if (skyHole->formedAt < 0.0f && skyHole->previousCollapse >= 0.0f && skyHole->previousCollapse < 1.0f) {
                /* the moment within this frame it reached 1 (not the frame's start: played or jumped
                 * to, the same moment then looks the same) */
                float rate = skyHole->collapseSpeed / 60.0f;
                skyHole->formedAt = rate > 0.0f ? skyHole->playTime + (1.0f - skyHole->previousCollapse) / rate
                                                : skyHole->playTime;
            }
            skyHole->previousCollapse = collapse;
            if (!skyHole->driven) skyHole->playTime += dt;      /* (driven: it is the timeline's clock) */
            float playTime = skyHole->playTime;
            skyShader->updateUniform("collapseTime", playTime);
            float formed = skyHole->formedAt >= 0.0f ? playTime - skyHole->formedAt : -1.0f;
            skyShader->updateUniform("formedTime", formed);
            /* the debris shell: the timeline's, or following the moon (reaching us DEBRIS_ARRIVAL
             * seconds after it formed, then thinning out over 4 more) */
            const float ARRIVAL = BlackHole::DEBRIS_ARRIVAL;
            float shell = m_debrisShell >= 0.0f ? m_debrisShell
                        : formed < 0.0f ? 0.0f
                        : formed <= ARRIVAL ? formed / ARRIVAL : std::min(1.0f + (formed - ARRIVAL) / 4.0f, 2.0f);
            skyShader->updateUniform("debrisShell", shell);
            m_shownShell = shell;
            float rate = skyHole->driven && skyHole->drivenRate > 0.0f ? skyHole->drivenRate : skyHole->collapseSpeed / 60.0f;
            skyShader->updateUniform("collapseRate", rate);
            /* matter thrown straight at us: the timeline's, or following the moon (as the last
             * of it falls in, until DEBRIS_ARRIVAL seconds after it formed: not the timeline's
             * shell, each is on its own) */
            m_frontEjecta = m_ejecta >= 0.0f ? m_ejecta
                          : glm::smoothstep(0.85f, 0.95f, collapse) *
                            (formed < 0.0f ? (collapse >= 1.0f ? 0.0f : 1.0f)
                                           : 1.0f - glm::smoothstep(ARRIVAL - 1.5f, ARRIVAL, formed));
            skyShader->updateUniform("frontEjecta", m_frontEjecta);
            /* the ignition flash and its shock ring: by the destruction's own clock (seconds
             * since it ignited = how far it has got), so it stops when that is paused and is
             * the same wherever it is looked at; dragged by hand (no speed): in real time */
            float flashRate = skyHole->driven && skyHole->drivenRate > 0.0f ? skyHole->drivenRate : skyHole->collapseSpeed / 60.0f;
            float flash = flashRate > 0.0f
                ? (collapse > IGNITION ? std::exp(-collapse / flashRate * 1.4f) : 0.0f)
                : std::exp(-std::max(m_renderTime - skyHole->ignitionTime, 0.0f) * 1.4f);
            skyShader->updateUniform("collapseFlash", flash);
            /* the jets leave along the spin axis (the disk's normal), as seen on the moon's disc */
            glm::vec3 moonDir = glm::normalize(m_environment.moonDirection);
            glm::vec3 right = glm::normalize(glm::cross(moonDir, std::abs(moonDir.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
            glm::vec3 up = glm::cross(right, moonDir);
            glm::vec3 axis = skyHole->diskNormal();
            glm::vec2 jet(glm::dot(axis, right), glm::dot(axis, up));
            jet = glm::length(jet) > 1e-3f ? glm::normalize(jet) : glm::vec2(0.0f, 1.0f);
            skyShader->updateUniform("collapseSite", (void *) &skyHole->collapseSite);
            skyShader->updateUniform("holeHorizon", skyHole->horizon);
            skyShader->updateUniform("jetAxis", (void *) &jet);
        }
        m_lastCollapseTime = m_renderTime;
        skyShader->updateUniform("collapse", collapse);
        if (collapse < 0.0f) {
            float noFlash = 0.0f, notFormed = -1.0f;
            m_frontEjecta = 0.0f;
            m_shownShell = 0.0f;
            skyShader->updateUniform("collapseFlash", noFlash);
            skyShader->updateUniform("formedTime", notFormed);
        }
        float clear = skyHole != nullptr ? skyHole->cloudClear : 0.0f;
        skyShader->updateUniform("holeCloudClear", clear);
        skyShader->updateUniform("cloudPass", 0);
        float stream = skyHole != nullptr && !skyHole->kugelblitz ? skyHole->moonStream : 0.0f;
        skyShader->updateUniform("moonStream", stream);
        if (skyHole != nullptr) {
            glm::vec3 holeDirection = glm::normalize(skyHole->direction);
            glm::vec3 holeDiskNormal = skyHole->diskNormal();
            float diskAngle = skyHole->diskOuterAngle();
            skyShader->updateUniform("blackHoleDirection", (void *) &holeDirection);
            skyShader->updateUniform("blackHoleDiskNormal", (void *) &holeDiskNormal);
            skyShader->updateUniform("blackHoleDiskAngle", diskAngle);
            skyShader->updateUniform("blackHoleDiskColor", (void *) &skyHole->diskColor);
            /* the clouds are cut out where the hole bends the sky (a bit beyond
             * its reach: rays grazing the shadow look further out) */
            float lensAngle = std::asin(std::min(skyHole->influence * std::tan(skyHole->angularRadius), 0.99f));
            glm::vec2 cutCos(std::cos(std::min(lensAngle * 1.3f, 1.3f)), std::cos(std::min(lensAngle * 1.6f, 1.5f)));
            skyShader->updateUniform("cloudCutDirection", (void *) &holeDirection);
            skyShader->updateUniform("cloudCutCos", (void *) &cutCos);
        }
    }
    skyShader->updateUniform("cameraPosition", (void *) &camera->getPosition());
    DirectionalLight::getLightAccumulationShader().updateUniform("cloudTime", m_renderTime);
    uploadWind(deferredShading_InstanceRender);
    uploadWind(depthMapCreatorInstanceRender);
    DirectionalLight::getLightAccumulationShader().updateUniform("inverseViewMatrix", (void *) &inverseViewMatrix);
    PointLight::getLightAccumulationShader().updateUniform("inverseViewMatrix", (void *) &inverseViewMatrix);
    deferredShading_BufferCombinationShader.updateUniform("inverseViewMatrix", (void *) &inverseViewMatrix);
    SpotLight::getLightAccumulationShader().updateUniform("inverseViewMatrix", (void *) &inverseViewMatrix);
    volumetricLightShader.updateUniform("inverseViewMatrix", (void *) &inverseViewMatrix);

    DirectionalLight::getLightAccumulationShader().updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    DirectionalLight::getLightAccumulationShader().updateUniform("cameraForwardVectorEyeSpace", (void *) &forwardVectorInEyeSpace);
    DirectionalLight::getLightAccumulationShader().updateUniform("cameraForwardVector", (void *) &cameraForward);

    SpotLight::getLightAccumulationShader().updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    SpotLight::getLightAccumulationShader().updateUniform("cameraForwardVectorEyeSpace", (void *) &forwardVectorInEyeSpace);
    SpotLight::getLightAccumulationShader().updateUniform("cameraForwardVector", (void *) &cameraForward);

    PointLight::getLightAccumulationShader().updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    PointLight::getLightAccumulationShader().updateUniform("cameraForwardVectorEyeSpace", (void *) &forwardVectorInEyeSpace);
    PointLight::getLightAccumulationShader().updateUniform("cameraForwardVector", (void *) &cameraForward);

    deferredShading_StencilBufferCreator.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    deferredShading_SceneShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    deferredShading_AnimRender.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    deferredShading_InstanceRender.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    skyShader->updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    volumetricLightShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    flareShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    DebugRenderer::lineShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);

    if (smokeRenderer != NULL) {
        smokeRenderer->getRenderingShader().updateUniform("viewMatrix", (void *) &cameraViewMatrix);
        smokeRenderer->getRenderingShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    }

    for (Light *light : lights) {
        if (light->isCastingShadow()) {
            light->recomputeShadowMapViewMatrix();
            light->recomputeShadowMapProjectionMatrix();
        }
    }
}

void RenderingMaster::computeStencilBufferForLight(Light *light)
{
    gBuffer.unbind();
    gBuffer.bindForStencil();

    light->prepareOpenGLForStencilPass();
    light->render(deferredShading_StencilBufferCreator);
}

void RenderingMaster::computeLightAccumulationBufferForLight(Light *light)
{
    gBuffer.unbind();
    gBuffer.bindForLights();

    light->prepareOpenGLForLightPass();

    normalTexture->use(1);
    depthTexture->use(2);
    roughnessTexture->use(3);
    blueNoiseTexture->use(4);
    albedoTexture->use(6); /* PBR light passes apply the surface color themselves */

    light->render();
}

void RenderingMaster::createLightAccumulationBuffer()
{
    gBuffer.unbind();
    gBuffer.bindForLights();
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (Light *light : lights) {
        /* lights whose whole volume is past the draw distance light nothing visible */
        if (m_environment.fogEnd > 0.0f) {
            float reach = -1.0f;
            if (PointLight *p = dynamic_cast<PointLight *>(light)) reach = p->getRange();
            else if (dynamic_cast<SpotLight *>(light) != NULL) reach = light->getTransform().getScale().z;
            if (reach > 0.0f && isBeyondDrawDistance(light->getTransform().getPosition(), reach)) {
                continue;
            }
        }
        if (light->needsStencilTest()) {
            computeStencilBufferForLight(light);
        }
        computeLightAccumulationBufferForLight(light);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_CLAMP);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
}

void RenderingMaster::beginCreateDepthTextureForLight(Light *light)
{
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    light->getShadowMapFrameBuffer().bindAllRenderTargets();

    depthMapCreator.updateUniform("viewMatrix", (void *) &light->getShadowMapViewMatrix());
    depthMapCreator.updateUniform("projectionMatrix", (void *) &light->getShadowMapProjectionMatrix());

    depthMapCreatorInstanceRender.updateUniform("viewMatrix", (void *) &light->getShadowMapViewMatrix());
    depthMapCreatorInstanceRender.updateUniform("projectionMatrix", (void *) &light->getShadowMapProjectionMatrix());
}

void RenderingMaster::endCreateDepthTextureForLight(Light *light) {
    light->getShadowMapFrameBuffer().unbind();
    glCullFace(GL_BACK);
}

void RenderingMaster::addLightToScene(Light *light)
{
    lights.push_back (light);
}

void RenderingMaster::removeLightFromScene(Light *light)
{
    auto it = std::find(lights.begin(), lights.end(), light);
    if (it == lights.end()) {
        return;
    }
    lights.erase(it);
    m_removedLights.push_back(light);
}

void RenderingMaster::deleteRemovedLights()
{
    for (Light *light : m_removedLights) {
        delete light;
    }
    m_removedLights.clear();
}

void RenderingMaster::resetLights()
{
    deleteRemovedLights();
    auto it = lights.begin();
    for (; it != lights.end();) {
        if (dynamic_cast<DirectionalLight*>(*it) == nullptr) {
            delete *it;
            it = lights.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<Light *> &RenderingMaster::getLights()
{
    return lights;
}

void RenderingMaster::drawMainSky()
{
    /* drawn after the scene, at the far plane: the depth test leaves it only the pixels nothing
     * else covers (the sky is the costliest thing to shade; covered pixels were shaded for
     * nothing). Depth clamp: the far plane itself is not clipped away */
    skyShader->updateUniform("cloudPass", m_skyCloudCut ? 1 : 0);
    skyShader->updateUniform("farPlane", 1);
    glEnable(GL_DEPTH_TEST);
    GLint depthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_CLAMP);
    drawSky();
    glDisable(GL_DEPTH_CLAMP);
    glDepthFunc(depthFunc);
    skyShader->updateUniform("farPlane", 0);
    skyShader->updateUniform("cloudPass", 0);
}

void RenderingMaster::drawSky()
{
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    if (m_moonTexture != nullptr) {
        m_moonTexture->use(15);
        skyShader->updateUniform("moonSampler", 15);
    }
    dynamic_cast<RenderComponent *>(skyDomeEntity.getComponent(Entity::Flags::RENDERABLE))->render(skyShader);

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

void RenderingMaster::renderVolumetricLight()
{
    volumetricLightFB.bindAllRenderTargets();
    for (unsigned int i = 0; i < lights.size(); i++) {
        SpotLight *spot = NULL;
        if ((spot = dynamic_cast<SpotLight *>(lights[i])) != NULL && spot->isVolumetric() == true) {
            float alpha = spot->getConeAngle();
            float virtualSphereDiameter = lights[i]->getTransform().getScale().z * 2.0f;

            lights[i]->getShadowMapTexture().use(0);
            depthTexture->use(1);
            blueNoiseTexture->use(2);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            volumetricLightShader.updateUniform("volumetricLightProjectionMatrix", (void *) &lights[i]->getShadowMapProjectionMatrix());
            volumetricLightShader.updateUniform("volumetricLightViewMatrix", (void *) &lights[i]->getShadowMapViewMatrix());
            glm::mat4 volumetricLightViewProj = lights[i]->getShadowMapProjectionMatrix() * lights[i]->getShadowMapViewMatrix();
            volumetricLightShader.updateUniform("volumetricLightViewProj", (void *) &volumetricLightViewProj);
            volumetricLightShader.updateUniform("lightColor", (void *) &lights[i]->getLightColor());
            volumetricLightShader.updateUniform("lightPosition", (void *) &lights[i]->getTransform().getPosition());
            volumetricLightShader.updateUniform("modelMatrix", (void *) &lights[i]->getTransform().getModelMatrix());
            volumetricLightShader.updateUniform("cameraPosition", (void *) &camera->getPosition());
            volumetricLightShader.updateUniform("alpha", alpha);
            volumetricLightShader.updateUniform("virtualSphereDiameter", virtualSphereDiameter);
            volumetricLightShader.updateUniform("numSamplePoints", spot->getNumSamplePoints());
            volumetricLightShader.updateUniform("coef1", spot->getCoef1());
            volumetricLightShader.updateUniform("coef2", spot->getCoef2());
            volumetricLightShader.updateUniform("beamDensity", spot->getBeamDensity());
            volumetricLightShader.updateUniform("beamFalloff", spot->getBeamFalloff());
            volumetricLightShader.updateUniform("beamHaze", spot->getBeamHaze());
            uploadFogCutoff(volumetricLightShader);
            volumetricLightShader.updateUniform("time", m_renderTime);
            volumetricLightShader.updateUniform("secondFraction", secondFraction);
            lights[i]->render(volumetricLightShader);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glCullFace(GL_BACK);
            glDisable(GL_BLEND);
        }
    }
    volumetricLightFB.unbind();
}

void RenderingMaster::startIMGUIFrame() {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(display->getWindow());
    ImGui::NewFrame();
}

void RenderingMaster::drawScreenQuad() {
    screenSizeRectangle->draw();
}

/* black holes bend the composited image: each reads a copy of it */
/* glass (glass.fs): each pane shows the composited image behind it, bent and
 * tinted, with reflections; drawn into the HDR image after the water */
void RenderingMaster::renderGlass(FrameBuffer &target)
{
    if (glass.empty()) {
        return;
    }
    if (m_glassShader == nullptr) {
        m_glassShader = new Shader("res/shaders/glass.json");
        m_glassShader->updateUniform("sceneSampler", 0);
        m_glassShader->updateUniform("depthSampler", 1);
        m_glassShader->updateUniform("skyCube", 2);
        m_glassCopyFB = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
    }
    /* the image behind the glass */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFrameBufferObject());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_glassCopyFB->getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    /* draw into the target as it is (bindSingleRenderTarget would clear it) */
    glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, renderWidth, renderHeight);
    Shader &shader = *m_glassShader;
    glm::vec2 screen((float) renderWidth, (float) renderHeight);
    shader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    shader.updateUniform("viewMatrix", (void *) &camera->getViewMatrix());
    shader.updateUniform("cameraPosition", (void *) &camera->getPosition());
    shader.updateUniform("screenSize", (void *) &screen);
    glm::vec3 floor = m_environment.ambientSky * 0.6f;
    shader.updateUniform("reflectionFloor", (void *) &floor);
    shader.bind();
    uploadGlowLights(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_glassCopyFB->getRenderTargets()[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture->getTextureId());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCube);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    for (Entity *entity : glass) {
        RenderComponent *rc = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
        if (rc != nullptr) {
            rc->render(&shader);
        }
    }
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glass.clear();
}

/* heat haze above fires (emitters with "heat"), nearest first, for the
 * tone mapping pass */
void RenderingMaster::uploadHeat(Shader &shader)
{
    const int MAX_HEAT = 8;
    glm::vec3 eye = camera->getPosition();
    std::vector<std::pair<float, ParticleEmitter *>> hot;
    for (ParticleEmitter *emitter : particleEmitters) {
        if (emitter->getHeat() > 0.0f) {
            hot.push_back({glm::length(emitter->getPosition() - eye), emitter});
        }
    }
    std::sort(hot.begin(), hot.end(), [](const std::pair<float, ParticleEmitter *> &a,
                                         const std::pair<float, ParticleEmitter *> &b) { return a.first < b.first; });
    int count = std::min((int) hot.size(), MAX_HEAT);
    glm::vec3 positions[MAX_HEAT], params[MAX_HEAT];
    for (int i = 0; i < count; i++) {
        ParticleEmitter *e = hot[i].second;
        positions[i] = e->getPosition();
        float radius = std::max(e->getRadius() * 2.5f, 6.0f);
        params[i] = glm::vec3(radius, radius * 9.0f, e->getHeat());   /* a column ~9 radii tall */
    }
    shader.updateUniform("heatCount", count);
    glm::mat4 inverseProjection = glm::inverse(projectionMatrix);
    glm::mat4 inverseView = glm::inverse(camera->getViewMatrix());
    shader.updateUniform("inverseProjectionMatrix", (void *) &inverseProjection);
    shader.updateUniform("inverseViewMatrix", (void *) &inverseView);
    float time = m_renderTime;
    shader.updateUniform("time", time);
    GLuint program = shader.getShaderId();
    GLint loc;
    if ((loc = glGetUniformLocation(program, "heatPositions")) >= 0 && count > 0)
        glUniform3fv(loc, count, &positions[0].x);
    if ((loc = glGetUniformLocation(program, "heatParams")) >= 0 && count > 0)
        glUniform3fv(loc, count, &params[0].x);
}

/* the point lights nearest the camera, for their glow in the fog
 * (bufferCombinationShader: lightGlow) */
void RenderingMaster::uploadGlowLights(Shader &shader)
{
    const int MAX_GLOW = 16;
    struct Glow { glm::vec3 position, color; float radius, distance; };
    std::vector<Glow> glows;
    glm::vec3 eye = camera->getPosition();
    for (Light *light : lights) {
        if (dynamic_cast<PointLight *>(light) == nullptr) continue;
        const glm::vec3 &color = light->getLightColor();
        if (color.r + color.g + color.b <= 0.003f) continue;
        Glow g;
        g.position = light->getTransform().getPosition();
        g.radius = light->getTransform().getScale().x * 0.75f;   /* the deferred pass's cut-off */
        g.color = color;
        g.distance = glm::length(g.position - eye) - g.radius;
        glows.push_back(g);
    }
    std::sort(glows.begin(), glows.end(), [](const Glow &a, const Glow &b) { return a.distance < b.distance; });
    int count = std::min((int) glows.size(), MAX_GLOW);
    glm::vec3 positions[MAX_GLOW], colors[MAX_GLOW];
    float radii[MAX_GLOW];
    for (int i = 0; i < count; i++) {
        positions[i] = glows[i].position;
        colors[i] = glows[i].color;
        radii[i] = glows[i].radius;
    }
    shader.updateUniform("glowLightCount", count);
    GLuint program = shader.getShaderId();
    GLint loc;
    if ((loc = glGetUniformLocation(program, "glowLightPositions")) >= 0 && count > 0)
        glUniform3fv(loc, count, &positions[0].x);
    if ((loc = glGetUniformLocation(program, "glowLightColors")) >= 0 && count > 0)
        glUniform3fv(loc, count, &colors[0].x);
    if ((loc = glGetUniformLocation(program, "glowLightRadii")) >= 0 && count > 0)
        glUniform1fv(loc, count, radii);
}

void RenderingMaster::renderBlackHoles(FrameBuffer &target)
{
    if (blackHoles.empty()) {
        return;
    }
    if (m_lensCopyFB == nullptr) {
        m_lensCopyFB = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
    }
    GLuint sky = m_environment.pbr ? m_skyCube : 0;
    for (BlackHole *hole : blackHoles) {
        GLuint lensSky = 0;
        if (!hole->drawsAnything()) {
            continue;
        }
        if (hole->sky) {
            if (!hole->skyMayBeVisible(camera->getForward(), projectionMatrix)) {
                continue;
            }
            PT_FromHere("blackHoles.lensSky");
            renderLensSky(*hole);
            PT_ToHere("blackHoles.lensSky");
            lensSky = m_lensSkyFB->getRenderTargets()[0];
        }
        PT_FromHere("blackHoles.copy");
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFrameBufferObject());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_lensCopyFB->getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        /* mipmaps: the lens samples it minified near the Einstein ring */
        glBindTexture(GL_TEXTURE_2D, m_lensCopyFB->getRenderTargets()[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, renderWidth, renderHeight);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        PT_ToHere("blackHoles.copy");
        PT_FromHere("blackHoles.march");
        hole->render(m_lensCopyFB->getRenderTargets()[0], depthTexture->getTextureId(), sky,
                     renderWidth, renderHeight, lensSky,
                     m_lensSurroundingsDrawn && !hole->sky ? m_lensSurroundings : nullptr);
        PT_ToHere("blackHoles.march");
    }
    PT_FromHere("blackHoles.inFront");
    /* (drawn over everything: not left to whatever the holes above set, as none may have drawn) */
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if (m_skyCloudCut) {
        /* the clouds in front of a sky black hole, over what it bent */
        glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, renderWidth, renderHeight);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        depthTexture->use(13);
        glm::mat4 cameraView = camera->getViewMatrix();
        glm::vec2 screenSize((float) renderWidth, (float) renderHeight);
        skyShader->updateUniform("viewMatrix", (void *) &cameraView);
        skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);
        skyShader->updateUniform("sceneDepth", 13);
        skyShader->updateUniform("screenSize", (void *) &screenSize);
        skyShader->updateUniform("cloudPass", 2);
        drawSky();
        skyShader->updateUniform("cloudPass", 0);
        glDisable(GL_BLEND);
    }
    if (m_frontEjecta > 0.0f) {
        /* the moon's matter flying straight at us: in front of the hole */
        glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, renderWidth, renderHeight);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);
        depthTexture->use(13);
        glm::mat4 cameraView = camera->getViewMatrix();
        glm::vec2 screenSize((float) renderWidth, (float) renderHeight);
        skyShader->updateUniform("viewMatrix", (void *) &cameraView);
        skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);
        skyShader->updateUniform("sceneDepth", 13);
        skyShader->updateUniform("screenSize", (void *) &screenSize);
        skyShader->updateUniform("cloudPass", 4);
        drawSky();
        skyShader->updateUniform("cloudPass", 0);
        glDisable(GL_BLEND);
    }
    PT_ToHere("blackHoles.inFront");
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void RenderingMaster::renderBlackHolesMirrored(FrameBuffer &target, const glm::mat4 &mirroredView,
                                               const glm::vec3 &mirroredEye)
{
    if (blackHoles.empty()) {
        return;
    }
    int w = target.getWidth(), h = target.getHeight();
    if (m_mirrorLensCopyFB == nullptr || m_mirrorLensCopyFB->getWidth() != w || m_mirrorLensCopyFB->getHeight() != h) {
        delete m_mirrorLensCopyFB;
        m_mirrorLensCopyFB = new FrameBuffer(w, h, 1, false, GL_RGBA16F);
    }
    if (m_farDepthTexture == 0) {
        /* only the sky is in the reflection yet: nothing in front of the holes */
        const float far = 1.0f;
        glGenTextures(1, &m_farDepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_farDepthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &far);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    GLuint sky = m_environment.pbr ? m_skyCube : 0;
    GLint frontFace;
    glGetIntegerv(GL_FRONT_FACE, &frontFace);
    glFrontFace(GL_CCW);                    /* the screen quad, not mirrored */
    /* the mirror shows the sky the other way up: what is in it is what a camera looking the
     * mirrored way would see */
    glm::vec3 mirroredForward = camera->getForward() * glm::vec3(1.0f, -1.0f, 1.0f);
    for (BlackHole *hole : blackHoles) {
        GLuint lensSky = 0;
        if (!hole->drawsAnything()) {
            continue;
        }
        if (hole->sky) {
            if (!hole->skyMayBeVisible(mirroredForward, projectionMatrix)) {
                continue;
            }
            PT_FromHere("waterReflection.blackHoles.lensSky");
            renderLensSky(*hole);
            PT_ToHere("waterReflection.blackHoles.lensSky");
            lensSky = m_lensSkyFB->getRenderTargets()[0];
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.getFrameBufferObject());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_mirrorLensCopyFB->getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, m_mirrorLensCopyFB->getRenderTargets()[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, w, h);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        PT_FromHere("waterReflection.blackHoles.march");
        hole->render(m_mirrorLensCopyFB->getRenderTargets()[0], m_farDepthTexture, sky, w, h, lensSky,
                     mirroredView, mirroredEye);
        PT_ToHere("waterReflection.blackHoles.march");
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glFrontFace(frontFace);
    /* back to the reflection, as it was bound (without clearing it) */
    glBindFramebuffer(GL_FRAMEBUFFER, target.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, w, h);
}

void RenderingMaster::renderLensSurroundings(const std::vector<Entity *> &entities)
{
    m_lensSurroundingsDrawn = false;
    bool worldHole = false;
    for (BlackHole *hole : blackHoles) {
        worldHole |= !hole->sky;
    }
    if (!worldHole) {
        return;
    }
    if (m_lensSurroundings == nullptr) {
        m_lensSurroundings = new LensSurroundings();
    }
    PT_FromHere("lensSurroundings");
    m_lensSurroundingsDrawn = m_lensSurroundings->render(entities, blackHoles);
    PT_ToHere("lensSurroundings");
}

/* the sky behind a sky black hole: stars, moon and the moon stream, without
 * the clouds (they are in front of it), sharper than the screen around it */
void RenderingMaster::renderLensSky(const BlackHole &hole)
{
    if (m_lensSkyFB == nullptr) {
        m_lensSkyFB = new FrameBuffer(LENS_SKY_SIZE, LENS_SKY_SIZE, 1, false, GL_RGBA16F);
    }
    if (m_lensSkyHole == &hole && m_lensSkyFrame == m_frameNumber) {
        return;                 /* already drawn this frame (for the water's mirror image) */
    }
    m_lensSkyHole = &hole;
    m_lensSkyFrame = m_frameNumber;
    glm::mat4 view = hole.lensSkyView();
    glm::mat4 projection = hole.lensSkyProjection();
    m_lensSkyFB->bindSingleRenderTarget(0);
    glViewport(0, 0, LENS_SKY_SIZE, LENS_SKY_SIZE);
    glDisable(GL_BLEND);
    skyShader->updateUniform("viewMatrix", (void *) &view);
    skyShader->updateUniform("projectionMatrix", (void *) &projection);
    skyShader->updateUniform("cloudPass", 3);
    drawSky();
    skyShader->updateUniform("cloudPass", 0);
    glm::mat4 cameraView = camera->getViewMatrix();
    skyShader->updateUniform("viewMatrix", (void *) &cameraView);
    skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);

    glBindTexture(GL_TEXTURE_2D, m_lensSkyFB->getRenderTargets()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, renderWidth, renderHeight);
}

void RenderingMaster::blackHoleGUI()
{
    ImGui::Begin("Black holes");
    if (ImGui::Button("Add in front of me")) {
        BlackHole *hole = new BlackHole(camera->getPosition() + camera->getForward() * 600.0f);
        blackHoles.push_back(hole);
    }
    for (size_t i = 0; i < blackHoles.size(); i++) {
        if (!blackHoles[i]->renderGUI((int) i)) {
            delete blackHoles[i];
            blackHoles.erase(blackHoles.begin() + i);
            i--;
        }
    }
    ImGui::End();
}

void RenderingMaster::imguiDrawCalls() {
    environmentGUI();
    blackHoleGUI();
    {
        ImGui::Begin("GUI!");

        //ImGui::Text("This is some useful text.");
        for (int i = 0; i < GUIVarsEnum_f::NUM_VARS_f; i++) {
            std::string name("float ");
            name += i+'0';
            ImGui::SliderFloat(name.c_str(), &data_f[i], 0.0f, 10.0f);
        }

        for (int i = 0; i < GUIVarsEnum_vec3::NUM_VARS_v3; i++) {
            std::string name("vec3 ");
            name += i+'0';
            ImGui::ColorEdit3(name.c_str(), (float *) &data_vec3[i]);
        }

        for (int i = 0; i < GUIVarsEnum_int::NUM_VARS_i; i++) {
            if (ImGui::Button("Button"))
                data_i[i]++;

            ImGui::SameLine();
            ImGui::Text("counter_%d = %d", i, data_i[i]);
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        static char str0[128] = "Hello, world!";
        ImGui::InputText("input text", str0, sizeof(str0));
        ImGui::End();
    }
}

void RenderingMaster::renderIMGUI() {
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void RenderingMaster::setFOV(float fov) {
    float aspectRatio = (float) display->getWidth() / (float) display->getHeight();
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, 1.0f, m_farPlane);
    uploadProjection();
}

void RenderingMaster::setFarPlane(float farPlane) {
    float nearPlane = projectionMatrix[3][2] / (projectionMatrix[2][2] - 1.0f);
    m_farPlane = farPlane;
    projectionMatrix[2][2] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    projectionMatrix[3][2] = -2.0f * farPlane * nearPlane / (farPlane - nearPlane);
    uploadProjection();
}

bool RenderingMaster::isBeyondDrawDistance(const glm::vec3 &center, float radius) const {
    if (m_environment.fogEnd <= 0.0f) {
        return false;
    }
    return glm::length(center - camera->getPosition()) - radius > m_farPlane;
}

void RenderingMaster::uploadFogCutoff(Shader &shader) const {
    float start = m_environment.fogStart, end = m_environment.fogEnd;
    shader.updateUniform("fogStart", start);
    shader.updateUniform("fogEnd", end);
}

glm::vec2 RenderingMaster::getWindDirection() const {
    /* the wind blows the way the clouds drift (or, in the Kugelblitz
     * aftermath, from the black hole) */
    glm::vec2 direction = glm::normalize(glm::vec2(0.8f, 0.35f));
    if (m_stormTurn > 0.0f) {
        glm::vec2 turned = glm::mix(direction, m_stormDirection, m_stormTurn);
        direction = glm::length(turned) > 1e-3f ? glm::normalize(turned) : m_stormDirection;
    }
    return direction * m_environment.wind;
}

void RenderingMaster::uploadWind(Shader &shader) const {
    glm::vec2 direction = getWindDirection();
    float time = m_renderTime;
    shader.updateUniform("windDirection", (void *) &direction);
    shader.updateUniform("windTime", time);
}

void RenderingMaster::uploadProjection() {
    bool result = true;
    result &= deferredShading_SceneShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= deferredShading_InstanceRender.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= deferredShading_AnimRender.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= SpotLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= PointLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= deferredShading_StencilBufferCreator.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= volumetricLightShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= flareShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);

    glm::mat4 inverseProjection = glm::inverse(projectionMatrix);
    deferredShading_BufferCombinationShader.updateUniform("inverseProjectionMatrix", (void *) &inverseProjection);

    assert(result);
}

void RenderingMaster::setEnvironment(const Environment &environment)
{
    m_baseEnvironment = environment;
    m_timePaused = environment.timePaused;
    m_dirLightBaseCaptured = false;
    applyEnvironment(environment.dayLength > 0.0f ?
                     Environment::atTimeOfDay(environment, environment.timeOfDay) : environment);
}

void RenderingMaster::setTimeOfDay(float timeOfDay)
{
    if (m_baseEnvironment.dayLength > 0.0f) {
        applyEnvironment(Environment::atTimeOfDay(m_baseEnvironment, timeOfDay));
    }
}

/* ImGui window to change the time of day and the atmosphere while running
 * (press M to free the mouse cursor to use it) */
void RenderingMaster::environmentGUI()
{
    ImGui::SetNextWindowPos(ImVec2(display->getWidth() - 430.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(410.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("World");

    if (onSaveMap && ImGui::Button("Save map (F5)")) {
        onSaveMap();
    }
    ImGui::SameLine();
    if (onReloadMap && ImGui::Button("Reload saved (F9)")) {
        onReloadMap();
    }
    if (!savedMapPath.empty()) {
        ImGui::TextDisabled("%s", savedMapPath.c_str());
    }
    ImGui::Separator();

    bool changed = false;
    if (m_baseEnvironment.dayLength > 0.0f) {
        float t = m_environment.timeOfDay;
        int minutes = (int) (t * 24.0f * 60.0f) % (24 * 60);
        ImGui::Text("Clock  %02d:%02d", minutes / 60, minutes % 60);
        if (ImGui::SliderFloat("Time of day", &t, 0.0f, 1.0f, "%.3f")) {
            setTimeOfDay(t);
        }
        if (ImGui::Button("Midnight")) setTimeOfDay(0.0f);
        ImGui::SameLine();
        if (ImGui::Button("Dawn")) setTimeOfDay(0.26f);
        ImGui::SameLine();
        if (ImGui::Button("Noon")) setTimeOfDay(0.5f);
        ImGui::SameLine();
        if (ImGui::Button("Dusk")) setTimeOfDay(0.74f);

        ImGui::Checkbox("Pause time", &m_timePaused);
        ImGui::SliderFloat("Time speed", &m_timeScale, 0.0f, 120.0f, "%.1fx");
        ImGui::SliderFloat("Day length (s)", &m_baseEnvironment.dayLength, 10.0f, 3600.0f, "%.0f");
    } else {
        ImGui::TextWrapped("No day-night cycle on this map (set Environment.dayLength in the map JSON).");
    }

    ImGui::Separator();
    changed |= ImGui::SliderFloat("Fog density", &m_baseEnvironment.fogDensity, 0.0f, 0.01f, "%.4f");
    changed |= ImGui::SliderFloat("Fog height falloff", &m_baseEnvironment.fogHeightFalloff, 0.0f, 0.03f, "%.4f");
    changed |= ImGui::SliderFloat("Haze glow", &m_baseEnvironment.fogMoonGlow, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Light glow in fog", &m_baseEnvironment.fogLightGlow, 0.0f, 5.0f);
    changed |= ImGui::Checkbox("Reflections (screen space)", &m_baseEnvironment.reflections);
    changed |= ImGui::SliderFloat("Eye adaptation", &m_baseEnvironment.autoExposure, 0.0f, 1.0f);
    changed |= ImGui::Checkbox("Temporal anti-aliasing", &m_baseEnvironment.temporalAA);
    changed |= ImGui::SliderFloat("Lens dirt", &m_baseEnvironment.lensDirt, 0.0f, 2.0f);
    changed |= ImGui::SliderFloat("Colour fringes", &m_baseEnvironment.chromaticAberration, 0.0f, 2.0f);
    changed |= ImGui::SliderFloat("Film grain", &m_baseEnvironment.filmGrain, 0.0f, 0.15f);
    changed |= ImGui::SliderFloat("Clouds", &m_baseEnvironment.cloudCoverage, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::Checkbox("Moon", &m_baseEnvironment.moon);
    changed |= ImGui::SliderFloat("Milky Way", &m_baseEnvironment.milkyWay, 0.0f, 3.0f);
    changed |= ImGui::SliderFloat("Moon phase", &m_baseEnvironment.moonPhase, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("Moon size", &m_baseEnvironment.moonSize, 0.005f, 0.09f, "%.3f");
    changed |= ImGui::SliderFloat("Wind", &m_baseEnvironment.wind, 0.0f, 3.0f, "%.2f");
    if (m_baseEnvironment.cloudCoverage > 0.0f) {
        changed |= ImGui::SliderFloat("Cloud speed", &m_baseEnvironment.cloudSpeed, 0.0f, 80.0f, "%.0f");
        changed |= ImGui::SliderFloat("Cloud height", &m_baseEnvironment.cloudHeight, 500.0f, 40000.0f, "%.0f", 3.0f);
    }
    changed |= ImGui::SliderFloat("Fog end / draw distance", &m_baseEnvironment.fogEnd, 0.0f, 6000.0f,
                                  m_baseEnvironment.fogEnd > 0.0f ? "%.0f" : "off");
    if (m_baseEnvironment.fogEnd > 0.0f) {
        changed |= ImGui::SliderFloat("Fog start", &m_baseEnvironment.fogStart, 0.0f, m_baseEnvironment.fogEnd, "%.0f");
    }
    changed |= ImGui::SliderFloat("Ambient (night)", &m_baseEnvironment.ambient, 0.0f, 0.5f);
    if (m_baseEnvironment.dayLength > 0.0f) {
        changed |= ImGui::SliderFloat("Ambient (day)", &m_baseEnvironment.dayAmbient, 0.0f, 0.8f);
        changed |= ImGui::SliderFloat("Stars", &m_baseEnvironment.starIntensity, 0.0f, 3.0f);
    }
    ImGui::Separator();
    changed |= ImGui::Checkbox("PBR shading", &m_baseEnvironment.pbr);
    changed |= ImGui::Checkbox("HDR + filmic tone mapping", &m_baseEnvironment.hdr);
    if (m_baseEnvironment.hdr) {
        changed |= ImGui::SliderFloat("Exposure", &m_baseEnvironment.exposure, 0.2f, 4.0f);
        if (m_baseEnvironment.dayLength > 0.0f && m_baseEnvironment.dayExposure >= 0.0f) {
            changed |= ImGui::SliderFloat("Exposure (day)", &m_baseEnvironment.dayExposure, 0.2f, 4.0f);
        }
        changed |= ImGui::SliderFloat("Bloom", &m_baseEnvironment.bloomStrength, 0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Bloom threshold", &m_baseEnvironment.bloomThreshold, 0.2f, 3.0f);
        changed |= ImGui::SliderFloat("Saturation", &m_baseEnvironment.saturation, 0.0f, 1.5f);
        changed |= ImGui::SliderFloat("Colour grade", &m_baseEnvironment.grade, 0.0f, 1.0f);
    }
    changed |= ImGui::SliderFloat("Ambient occlusion", &m_baseEnvironment.aoStrength, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("AO radius", &m_baseEnvironment.aoRadius, 1.0f, 40.0f);
    if (changed) {
        applyEnvironment(m_baseEnvironment.dayLength > 0.0f ?
                         Environment::atTimeOfDay(m_baseEnvironment, m_environment.timeOfDay) : m_baseEnvironment);
    }

    ImGui::End();
}

void RenderingMaster::cycleTimeScale()
{
    m_timeScale = m_timeScale < 2.0f ? 10.0f : (m_timeScale < 20.0f ? 60.0f : 1.0f);
    std::cout << "Time of day speed: " << m_timeScale << "x" << std::endl;
}

float RenderingMaster::getNightFactor() const
{
    if (m_baseEnvironment.dayLength <= 0.0f) {
        return 1.0f;
    }
    return 1.0f - m_environment.sunVisibility;
}

std::string RenderingMaster::stormState()
{
    /* the eye adaptation's values (fast, usual, broken pixels seen lately) */
    float adapted[4] = { 0, 0, 0, 0 };
    if (m_adaptationA != nullptr) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_adaptationA->getFrameBufferObject());
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, adapted);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }
    auto number = [](float x) { return std::isfinite(x) ? x : -999.0f; };     /* JSON has no NaN */
    char text[640];
    std::snprintf(text, sizeof(text),
                  "\"shell\": %.3f, \"ejecta\": %.3f, \"wind\": %.3f, \"stormFog\": %.3f, \"stormClouds\": %.3f, \"waves\": %.3f, \"dust\": %.3f, \"veil\": %.3f, \"fade\": %.3f, \"rain\": %.3f, \"lightning\": %.3f, "
                  "\"fog\": %.5f, \"clouds\": %.3f, \"lights\": %d, \"emitters\": %d, "
                  "\"eye\": [%.4f, %.4f], \"brokenPixels\": %.2f, \"worldTime\": %.3f, \"worldPaused\": %s",
                  m_shownShell, m_frontEjecta, m_wind, m_fog, m_clouds, m_waves, m_dust, m_dustVeil, m_fadeOut, m_rain, m_lightning, m_environment.fogDensity,
                  m_environment.cloudCoverage, (int) lights.size(), (int) particleEmitters.size(),
                  number(adapted[0]), number(adapted[1]), number(adapted[2]), m_time, m_worldPaused ? "true" : "false");
    return text;
}

void RenderingMaster::applyEnvironment(const Environment &base)
{
    /* the aftermath's storm on top of the time of day */
    Environment environment = base;
    m_mapWind = base.wind;
    environment.wind = environment.wind * (1.0f - 0.85f * m_calm) + 4.5f * m_wind;
    environment.fogDensity *= 1.0f + 3.0f * m_fog;
    environment.fogHeightFalloff *= std::max(1.0f - 0.6f * m_fog, 0.05f);
    environment.cloudCoverage = std::min(1.0f, environment.cloudCoverage + 0.35f * m_clouds);
    environment.cloudSpeed *= 1.0f + 3.0f * m_clouds;
    /* the storm's dust: a thick brown fog, the same at every height */
    if (m_dust > 0.0f) {
        environment.fogDensity = glm::mix(environment.fogDensity, 0.02f, m_dust * m_dust);
        environment.fogHeightFalloff *= 1.0f - m_dust;
        environment.fogColor = glm::mix(environment.fogColor, glm::vec3(0.11f, 0.08f, 0.055f), m_dust);
    }
    if (!environment.moon) {
        environment.moonColor = glm::vec3(0.0f);   /* no halo, no moonlit clouds or haze */
        environment.fogMoonGlow = 0.0f;
    }
    const float applied[5] = { m_wind, m_fog, m_clouds, m_calm, m_dust };
    std::copy(applied, applied + 5, m_appliedWeather);
    m_environment = environment;
    const Environment &e = m_environment;

    skyShader->updateUniform("skyMode", e.skyMode);
    skyShader->updateUniform("zenithColor", (void *) &e.zenithColor);
    skyShader->updateUniform("horizonColor", (void *) &e.horizonColor);
    skyShader->updateUniform("starIntensity", e.starIntensity);
    skyShader->updateUniform("moonDirection", (void *) &e.moonDirection);
    skyShader->updateUniform("moonColor", (void *) &e.moonColor);
    float moonPhase = e.moonPhase;
    skyShader->updateUniform("moonPhase", moonPhase);
    float moonSize = glm::clamp(e.moonSize, 0.005f, 0.09f);
    skyShader->updateUniform("moonSize", moonSize);
    float moonVisible = e.moon ? 1.0f : 0.0f;
    skyShader->updateUniform("moonVisible", moonVisible);
    float milkyWay = e.milkyWay;
    skyShader->updateUniform("milkyWay", milkyWay);
    glm::vec3 galaxyPole = glm::length(e.milkyWayNormal) > 1e-4f ? glm::normalize(e.milkyWayNormal) : glm::vec3(0, 1, 0);
    glm::vec3 galaxyCore = glm::length(e.milkyWayCenter) > 1e-4f ? glm::normalize(e.milkyWayCenter) : glm::vec3(0, 0, -1);
    skyShader->updateUniform("milkyWayNormal", (void *) &galaxyPole);
    skyShader->updateUniform("milkyWayCenter", (void *) &galaxyCore);

    Shader &c = deferredShading_BufferCombinationShader;
    c.updateUniform("ambient", e.ambient);
    bool pbr = e.pbr;
    c.updateUniform("pbrEnabled", pbr);
    deferredShading_SceneShader.updateUniform("pbrEnabled", pbr);
    deferredShading_InstanceRender.updateUniform("pbrEnabled", pbr);
    SpotLight::getLightAccumulationShader().updateUniform("pbrEnabled", pbr);
    PointLight::getLightAccumulationShader().updateUniform("pbrEnabled", pbr);
    DirectionalLight::getLightAccumulationShader().updateUniform("pbrEnabled", pbr);
    c.updateUniform("ambientSky", (void *) &e.ambientSky);
    c.updateUniform("ambientGround", (void *) &e.ambientGround);
    c.updateUniform("emissiveStrength", e.emissiveStrength);
    c.updateUniform("fogColor", (void *) &e.fogColor);
    c.updateUniform("fogDensity", e.fogDensity);
    c.updateUniform("fogHeightFalloff", e.fogHeightFalloff);
    c.updateUniform("fogBaseHeight", e.fogBaseHeight);
    c.updateUniform("fogMoonGlow", e.fogMoonGlow);
    c.updateUniform("fogLightGlow", e.fogLightGlow);
    uploadFogCutoff(c);
    {
        float coverage = e.cloudCoverage;
        glm::vec2 wind = glm::vec2(0.8f, 0.35f) * e.cloudSpeed;
        skyShader->updateUniform("cloudCoverage", coverage);
        skyShader->updateUniform("cloudWind", (void *) &wind);
        DirectionalLight::getLightAccumulationShader().updateUniform("cloudCoverage", coverage);
        DirectionalLight::getLightAccumulationShader().updateUniform("cloudWind", (void *) &wind);
        float height = std::max(e.cloudHeight, 100.0f);
        skyShader->updateUniform("cloudHeight", height);
        DirectionalLight::getLightAccumulationShader().updateUniform("cloudHeight", height);
    }
    /* the draw distance ends where the fog is opaque */
    float farPlane = e.fogEnd > 0.0f ? std::max(e.fogEnd * 1.01f, 200.0f) : m_defaultFarPlane;
    if (std::abs(farPlane - m_farPlane) > 0.5f) {
        setFarPlane(farPlane);
    }
    /* haze glows toward the dominant sky light (sun by day, moon by night) */
    c.updateUniform("moonDirection", (void *) &e.lightDirection);
    c.updateUniform("moonColor", (void *) &e.lightColor);

    float sunVisibility = e.sunVisibility;
    float sunsetStrength = e.sunsetStrength;
    skyShader->updateUniform("sunDirection", (void *) &e.sunDirection);
    skyShader->updateUniform("sunColor", (void *) &e.sunColor);
    skyShader->updateUniform("sunVisibility", sunVisibility);
    skyShader->updateUniform("sunsetColor", (void *) &e.sunsetColor);
    skyShader->updateUniform("sunsetStrength", sunsetStrength);
}

Texture &RenderingMaster::getFlareTexture()
{
    return flarePostProcess->getResultingTexture();
}

void RenderingMaster::setFlareSourceCount(int count)
{
    /* keep running the passes for one extra frame after the last flare source
     * disappears so the flare buffers end up black */
    m_framesWithoutFlares = count > 0 ? 0 : std::min(m_framesWithoutFlares + 1, 2);
}

static const float SUN_DISTANCE = 3000.0f;

bool RenderingMaster::sunScreenPosition(glm::vec2 &uv)
{
    glm::vec4 clip = projectionMatrix * camera->getViewMatrix() *
                     glm::vec4(camera->getPosition() + m_environment.sunDirection * SUN_DISTANCE, 1.0f);
    if (clip.w <= 0.0f) {
        return false;
    }
    uv = glm::vec2(clip) / clip.w * 0.5f + 0.5f;
    return uv.x > -0.25f && uv.x < 1.25f && uv.y > -0.25f && uv.y < 1.25f;
}

bool RenderingMaster::isSunFlareVisible()
{
    glm::vec2 uv;
    return m_baseEnvironment.dayLength > 0.0f && m_environment.sunVisibility > 0.01f && sunScreenPosition(uv);
}

void RenderingMaster::drawSunFlareSource()
{
    if (!isSunFlareVisible()) {
        return;
    }
    if (m_sunDisc == NULL) {
        m_sunDisc = Mesh::getCircle(0, 0, 1.0f, 32);
    }
    /* camera-facing disc far along the sun direction; the flare shader drops
     * the pixels where scene geometry is in front of it */
    glm::mat4 rotation = glm::transpose(glm::mat4(glm::mat3(camera->getViewMatrix())));
    /* inside the far plane (a map's fog cut-off can bring it closer than
     * SUN_DISTANCE): otherwise the disc is clipped when the sun is near the
     * middle of the screen and pops back in near the edges, where its view
     * depth (distance * cos(angle off-axis)) gets small enough */
    float distance = std::min(SUN_DISTANCE, m_farPlane * 0.7f);
    float radius = distance * glm::tan(glm::radians(1.3f));
    glm::mat4 model = glm::translate(glm::mat4(1.0f), camera->getPosition() + m_environment.sunDirection * distance) *
                      rotation * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
    glm::vec3 color = m_environment.sunColor * m_environment.sunVisibility;
    flareShader.updateUniform("modelMatrix", (void *) &model);
    flareShader.updateUniform("material.diffuse", (void *) &color);
    flareShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    flareShader.bind();
    glDisable(GL_CULL_FACE);
    m_sunDisc->draw();
    glEnable(GL_CULL_FACE);
}

static void bindTexture(int unit, GLuint texture) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}

void RenderingMaster::renderSkyCube()
{
    if (m_skyCube == 0) {
        glGenTextures(1, &m_skyCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCube);
        for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, SKY_CUBE_SIZE, SKY_CUBE_SIZE, 0,
                         GL_RGBA, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glGenFramebuffers(1, &m_skyCubeFbo);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    static const glm::vec3 directions[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
    static const glm::vec3 ups[6] = { {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0} };
    glm::mat4 cubeProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 200.0f);

    /* below the horizon: a dim ground color (no terrain in the reflection) */
    glm::vec3 ground = m_environment.ambientGround * 0.8f + m_environment.horizonColor * 0.2f;

    glBindFramebuffer(GL_FRAMEBUFFER, m_skyCubeFbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, SKY_CUBE_SIZE, SKY_CUBE_SIZE);
    skyShader->updateUniform("reflectionPass", true);
    skyShader->updateUniform("projectionMatrix", (void *) &cubeProjection);
    for (int face = 0; face < 6; face++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_skyCube, 0);
        glClearColor(ground.x, ground.y, ground.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f), directions[face], ups[face]);
        skyShader->updateUniform("viewMatrix", (void *) &view);
        drawSky();
    }
    skyShader->updateUniform("reflectionPass", false);
    skyShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);
    glm::mat4 cameraView = camera->getViewMatrix();
    skyShader->updateUniform("viewMatrix", (void *) &cameraView);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyCube);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void RenderingMaster::renderSSAO()
{
    int w = std::max(16, renderWidth / 2), h = std::max(16, renderHeight / 2);
    if (m_ssaoFB == nullptr) {
        m_ssaoFB = new FrameBuffer(w, h, 1);
        m_ssaoBlurFB = new FrameBuffer(w, h, 1);
        m_ssaoShader = new Shader("res/shaders/ssao.json");
        m_ssaoBlurShader = new Shader("res/shaders/ssaoBlur.json");
        m_ssaoShader->updateUniform("depthSampler", 0);
        m_ssaoShader->updateUniform("normalSampler", 1);
        m_ssaoShader->updateUniform("noiseSampler", 2);
        glm::vec2 noiseScale(w / 64.0f, h / 64.0f);
        m_ssaoShader->updateUniform("noiseScale", (void *) &noiseScale);
        m_ssaoBlurShader->updateUniform("aoSampler", 0);
        glm::vec2 texel(1.0f / w, 1.0f / h);
        m_ssaoBlurShader->updateUniform("texelSize", (void *) &texel);
    }
    glm::mat4 inverseProjection = glm::inverse(projectionMatrix);
    float radius = m_environment.aoRadius;
    m_ssaoShader->updateUniform("projectionMatrix", (void *) &projectionMatrix);
    m_ssaoShader->updateUniform("inverseProjectionMatrix", (void *) &inverseProjection);
    m_ssaoShader->updateUniform("radius", radius);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    m_ssaoFB->bindSingleRenderTarget(0);
    m_ssaoShader->bind();
    depthTexture->use(0);
    normalTexture->use(1);
    blueNoiseTexture->use(2);
    screenSizeRectangle->draw();
    m_ssaoFB->unbind();

    m_ssaoBlurFB->bindSingleRenderTarget(0);
    m_ssaoBlurShader->bind();
    bindTexture(0, m_ssaoFB->getRenderTargets()[0]);
    screenSizeRectangle->draw();
    m_ssaoBlurFB->unbind();
    glEnable(GL_DEPTH_TEST);
}

void RenderingMaster::blurPass(FrameBuffer &source, FrameBuffer &target, const glm::vec2 &direction)
{
    target.bindSingleRenderTarget(0);
    m_blurShader->updateUniform("direction", (void *) &direction);
    m_blurShader->bind();
    bindTexture(0, source.getRenderTargets()[0]);
    screenSizeRectangle->draw();
    target.unbind();
}

/* temporal anti-aliasing (taa.fs): blends this frame (in fxaaFrameBuffer)
 * with the reprojected history, and puts the result back for FXAA */
void RenderingMaster::renderTemporalAA()
{
    if (!m_environment.temporalAA || !m_environment.hdr) {
        m_taaHistoryValid = false;
        return;
    }
    if (m_taaShader == nullptr) {
        m_taaHistory[0] = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
        m_taaHistory[1] = new FrameBuffer(renderWidth, renderHeight, 1, false, GL_RGBA16F);
        m_taaShader = new Shader("res/shaders/taa.json");
        m_taaShader->updateUniform("currentSampler", 0);
        m_taaShader->updateUniform("historySampler", 1);
        m_taaShader->updateUniform("depthSampler", 2);
        m_taaHistoryValid = false;
    }
    /* this frame's clip space (jittered) -> world -> last frame's clip space (unjittered) */
    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 unjittered = projectionMatrix;
    unjittered[2][0] = unjittered[2][1] = 0.0f;
    glm::mat4 currentViewProjection = projectionMatrix * view;
    glm::mat4 reprojection = m_taaPreviousViewProjection * glm::inverse(currentViewProjection);
    m_taaPreviousViewProjection = unjittered * view;
    glm::vec2 texel(1.0f / renderWidth, 1.0f / renderHeight);
    float valid = m_taaHistoryValid ? 1.0f : 0.0f;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    FrameBuffer *target = m_taaHistory[m_taaIndex];
    FrameBuffer *history = m_taaHistory[1 - m_taaIndex];
    target->bindSingleRenderTarget(0);
    m_taaShader->bind();
    m_taaShader->updateUniform("reprojection", (void *) &reprojection);
    m_taaShader->updateUniform("texelSize", (void *) &texel);
    m_taaShader->updateUniform("historyValid", valid);
    /* (drawn over the finished picture: they fade out with it, or a fade to black is not black) */
    float grain = m_environment.filmGrain * (1.0f - m_fadeOut), time = m_renderTime;
    m_taaShader->updateUniform("filmGrain", grain);
    m_taaShader->updateUniform("time", time);
    float rain = m_rain * (1.0f - m_fadeOut);
    m_taaShader->updateUniform("rainAmount", rain);
    bindTexture(0, fxaaFrameBuffer.getRenderTargets()[0]);
    bindTexture(1, history->getRenderTargets()[0]);
    bindTexture(2, depthTexture->getTextureId());
    screenSizeRectangle->draw();
    target->unbind();
    /* back into the image FXAA reads */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target->getFrameBufferObject());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fxaaFrameBuffer.getFrameBufferObject());
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, renderWidth, renderHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_taaIndex = 1 - m_taaIndex;
    m_taaHistoryValid = true;
    glEnable(GL_DEPTH_TEST);
}

/* exposure adaptation: the scene's average luminance (log, mipmapped down to
 * one pixel), followed over time into m_adaptationA (see adaptation.fs) */
void RenderingMaster::updateAdaptation()
{
    if (m_environment.autoExposure <= 0.0f) {
        return;
    }
    const int SIZE = 64;
    if (m_luminanceFB == nullptr) {
        m_luminanceFB = new FrameBuffer(SIZE, SIZE, 1, false, GL_RGBA16F);
        m_adaptationA = new FrameBuffer(1, 1, 1, false, GL_RGBA16F);
        m_adaptationB = new FrameBuffer(1, 1, 1, false, GL_RGBA16F);
        m_luminanceShader = new Shader("res/shaders/luminance.json");
        m_adaptationShader = new Shader("res/shaders/adaptation.json");
        m_luminanceShader->updateUniform("hdrSampler", 0);
        m_adaptationShader->updateUniform("luminanceSampler", 0);
        m_adaptationShader->updateUniform("previousSampler", 1);
        /* the previous value starts empty (a = 0): the first frame settles */
        for (FrameBuffer *fb : {m_adaptationA, m_adaptationB}) {
            fb->bindSingleRenderTarget(0);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            fb->unbind();
        }
        GLuint luminance = m_luminanceFB->getRenderTargets()[0];
        glBindTexture(GL_TEXTURE_2D, luminance);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_lastAdaptationTime = SDL_GetTicks() / 1000.0;
    }
    m_luminanceFB->bindSingleRenderTarget(0);
    m_luminanceShader->bind();
    bindTexture(0, m_hdrFB->getRenderTargets()[0]);
    screenSizeRectangle->draw();
    m_luminanceFB->unbind();
    glBindTexture(GL_TEXTURE_2D, m_luminanceFB->getRenderTargets()[0]);
    glGenerateMipmap(GL_TEXTURE_2D);

    double realTime = SDL_GetTicks() / 1000.0;
    float dt = glm::clamp((float) (realTime - m_lastAdaptationTime), 0.0f, 0.25f);
    m_lastAdaptationTime = realTime;
    float lod = std::log2((float) SIZE);
    m_adaptationShader->updateUniform("luminanceLod", lod);
    m_adaptationShader->updateUniform("dt", dt);
    m_adaptationB->bindSingleRenderTarget(0);
    m_adaptationShader->bind();
    bindTexture(0, m_luminanceFB->getRenderTargets()[0]);
    bindTexture(1, m_adaptationA->getRenderTargets()[0]);
    screenSizeRectangle->draw();
    m_adaptationB->unbind();
    std::swap(m_adaptationA, m_adaptationB);
}

void RenderingMaster::renderBloomAndToneMap()
{
    int hw = std::max(16, renderWidth / 2), hh = std::max(16, renderHeight / 2);
    int qw = std::max(16, renderWidth / 4), qh = std::max(16, renderHeight / 4);
    if (m_tonemapShader == nullptr) {
        m_bloomHalfA = new FrameBuffer(hw, hh, 1, false, GL_RGBA16F);
        m_bloomHalfB = new FrameBuffer(hw, hh, 1, false, GL_RGBA16F);
        m_bloomQuarterA = new FrameBuffer(qw, qh, 1, false, GL_RGBA16F);
        m_bloomQuarterB = new FrameBuffer(qw, qh, 1, false, GL_RGBA16F);
        m_bloomBrightShader = new Shader("res/shaders/bloomBright.json");
        m_blurShader = new Shader("res/shaders/blur.json");
        m_tonemapShader = new Shader("res/shaders/tonemap.json");
        m_bloomBrightShader->updateUniform("hdrSampler", 0);
        m_blurShader->updateUniform("sourceSampler", 0);
        m_tonemapShader->updateUniform("hdrSampler", 0);
        m_tonemapShader->updateUniform("bloomSamplerNear", 1);
        m_tonemapShader->updateUniform("bloomSamplerFar", 2);
        m_tonemapShader->updateUniform("flareSampler", 3);
        m_tonemapShader->updateUniform("depthSampler", 4);
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    updateAdaptation();

    const bool bloom = m_environment.bloomStrength > 0.0f;
    if (bloom) {
        /* bright parts -> half res, blurred; then a wider blur at quarter res */
        float threshold = m_environment.bloomThreshold;
        m_bloomBrightShader->updateUniform("threshold", threshold);
        m_bloomHalfA->bindSingleRenderTarget(0);
        m_bloomBrightShader->bind();
        bindTexture(0, m_hdrFB->getRenderTargets()[0]);
        screenSizeRectangle->draw();
        m_bloomHalfA->unbind();
        blurPass(*m_bloomHalfA, *m_bloomHalfB, glm::vec2(1.0f / hw, 0.0f));
        blurPass(*m_bloomHalfB, *m_bloomHalfA, glm::vec2(0.0f, 1.0f / hh));
        blurPass(*m_bloomHalfA, *m_bloomQuarterA, glm::vec2(2.0f / hw, 0.0f));
        blurPass(*m_bloomQuarterA, *m_bloomQuarterB, glm::vec2(0.0f, 2.0f / qh));
    }

    float exposure = m_environment.exposure * (1.0f - m_fadeOut);
    float bloomStrength = bloom ? m_environment.bloomStrength : 0.0f;
    m_tonemapShader->updateUniform("exposure", exposure);
    m_tonemapShader->updateUniform("bloomStrength", bloomStrength);
    float saturation = m_environment.saturation, grade = m_environment.grade;
    m_tonemapShader->updateUniform("saturation", saturation);
    m_tonemapShader->updateUniform("gradeStrength", grade);
    fxaaFrameBuffer.bindSingleRenderTarget(0);
    m_tonemapShader->bind();
    bindTexture(0, m_hdrFB->getRenderTargets()[0]);
    bindTexture(1, m_bloomHalfA->getRenderTargets()[0]);
    bindTexture(2, m_bloomQuarterB->getRenderTargets()[0]);
    float flareStrength = blackHoles.empty() ? 0.0f : 1.0f;
    m_tonemapShader->updateUniform("flareStrength", flareStrength);
    uploadHeat(*m_tonemapShader);
    depthTexture->use(4);
    float autoExposure = m_adaptationA != nullptr ? m_environment.autoExposure : 0.0f;
    m_tonemapShader->updateUniform("autoExposure", autoExposure);
    m_tonemapShader->updateUniform("adaptationSampler", 5);
    if (m_adaptationA != nullptr) {
        bindTexture(5, m_adaptationA->getRenderTargets()[0]);
    }
    /* with temporal anti-aliasing the grain is added after it (renderTemporalAA) */
    bool taa = m_environment.temporalAA && m_environment.hdr;
    float lensDirt = m_environment.lensDirt, aberration = m_environment.chromaticAberration,
          grain = taa ? 0.0f : m_environment.filmGrain;
    glm::vec2 screen((float) renderWidth, (float) renderHeight);
    m_tonemapShader->updateUniform("lensDirt", lensDirt);
    m_tonemapShader->updateUniform("chromaticAberration", aberration);
    m_tonemapShader->updateUniform("filmGrain", grain);
    m_tonemapShader->updateUniform("screenSize", (void *) &screen);
    glm::vec3 dustColor(0.3f, 0.22f, 0.15f);
    m_tonemapShader->updateUniform("dustVeil", m_dustVeil);
    m_tonemapShader->updateUniform("dustColor", (void *) &dustColor);
    float rain = taa ? 0.0f : m_rain;       /* with TAA: added after it, like the grain */
    m_tonemapShader->updateUniform("rainAmount", rain);
    m_tonemapShader->updateUniform("lensWater", m_lensWater);
    m_tonemapShader->updateUniform("lightning", m_lightning);
    glm::vec3 bolt(m_boltX, m_boltSeed, m_boltReach);
    m_tonemapShader->updateUniform("bolt", (void *) &bolt);
    if (flareStrength > 0.0f) {
        flarePostProcess->getResultingTexture().use(3);
    }
    screenSizeRectangle->draw();
    glEnable(GL_DEPTH_TEST);
}

void RenderingMaster::drawParticles()
{
    if (smokeRenderer != NULL) {
        smokeRenderer->draw();
    }
    const glm::mat4 &view = camera->getViewMatrix();
    uploadFogCutoff(ParticleEmitter::getShader());
    for (ParticleEmitter *emitter : particleEmitters) {
        emitter->draw(projectionMatrix, view, renderWidth, renderHeight, m_renderAlpha);
    }
}
