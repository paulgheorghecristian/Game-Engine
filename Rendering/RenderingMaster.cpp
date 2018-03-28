#include "RenderingMaster.h"

#include "ParticleRenderer.h"

RenderingMaster *RenderingMaster::m_instance = NULL;

RenderingMaster::RenderingMaster(Display *display,
                                 Camera *camera,
                                 glm::mat4 projectionMatrix) : particleForwardRenderFramebuffer (display->getWidth(), display->getHeight(), 1),
                                                                particlesRTTexture (particleForwardRenderFramebuffer.getRenderTargets ()[0], 6) {
    int albedoTextureUnit = 0, normalTextureUnit = 1, lightAccumulationTextureUnit = 2;
    int depthTextureUnit = 3, outputType = 1, blurredLightAccUnit = 4, spotLightDepthMapUnit = 5;
    bool result = true;

    this->display = display;
    this->camera = camera;
    this->projectionMatrix = projectionMatrix;

    gBuffer.generate (display->getWidth(), display->getHeight());

    deferredShading_SceneShader.construct ("res/shaders/deferredShadingSceneShader.json");
    result &= deferredShading_SceneShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    deferredShading_BufferCombinationShader.construct ("res/shaders/bufferCombinationShader.json");
    result &= deferredShading_BufferCombinationShader.updateUniform("normalSampler", (void *) &normalTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("depthSampler", (void *) &depthTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("lightAccumulationSampler", (void *) &lightAccumulationTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("colorSampler", (void *) &albedoTextureUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("blurredLightAccSampler", (void *) &blurredLightAccUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform("outputType", (void *) &outputType);
    result &= deferredShading_BufferCombinationShader.updateUniform("spotLightDepthMap", (void *) &spotLightDepthMapUnit);
    result &= deferredShading_BufferCombinationShader.updateUniform ("particlesSampler", 6);
    assert (result);

    deferredShading_LightAccumulationBufferCreator.construct ("res/shaders/lightAccumulationBufferCreator.json");
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("eyeSpaceNormalSampler", (void *) &normalTextureUnit);
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("depthSampler", (void *) &depthTextureUnit);
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("screenWidth", (void *) &display->getWidth());
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("screenHeight", (void *) &display->getHeight());
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= deferredShading_LightAccumulationBufferCreator.updateUniform("spotLightDepthSampler", (void *) &spotLightDepthMapUnit);
    assert (result);

    deferredShading_StencilBufferCreator.construct ("res/shaders/stencilBufferCreator.json");
    result &= deferredShading_StencilBufferCreator.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    albedoTexture = new Texture (gBuffer.getColorTexture(), albedoTextureUnit);
    normalTexture = new Texture (gBuffer.getNormalTexture(), normalTextureUnit);
    lightAccumulationTexture = new Texture (gBuffer.getLightAccumulationTexture(), lightAccumulationTextureUnit);
    depthTexture = new Texture (gBuffer.getDepthTexture(), depthTextureUnit);

    brightnessControlPostProcess = new PostProcess (display->getWidth(), display->getHeight(),
                                                    gBuffer.getLightAccumulationTexture(),
                                                    "res/shaders/brightnessControlPostProcess.json");

    hBlurPostProcess = new PostProcess (display->getWidth()/8.0f, display->getHeight()/8.0f,
                                        brightnessControlPostProcess->getResultingTextureId(),
                                        "res/shaders/hBlurPostProcess.json");

    wBlurPostProcess = new PostProcess (display->getWidth()/8.0f, display->getHeight()/8.0f,
                                        hBlurPostProcess->getResultingTextureId(),
                                        "res/shaders/wBlurPostProcess.json");
    blurredLightAccTexture = new Texture (wBlurPostProcess->getResultingTextureId(), blurredLightAccUnit);
    screenSizeRectangle = Mesh::getRectangle();

    /*lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.9, 0, 0),
                                 Transform(glm::vec3(100.0f, 50.0f, 250.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.9f, 0.9f, 0.0f),
                                 Transform(glm::vec3(30.0f, 50.0f, 20.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.9f, 0.9f, 0.8f),
                                 Transform(glm::vec3(100.0f, 50.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));*/
    /*lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 0.0f, 0.5f),
                                 Transform(glm::vec3(200.0f, 100.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.6f, 1.0f, 0.0f),
                                 Transform(glm::vec3(70.0f, 200.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 1.0f, 0.7f),
                                 Transform(glm::vec3(10.0f, 250.0f, 100.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.0f, 0.6f, 0.7f),
                                 Transform(glm::vec3(70.0f, 200.0f, 200.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 1.0f, 1.0f),
                                 Transform(glm::vec3(70.0f, 209.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.0f, 1.0f, 0.0f),
                                 Transform(glm::vec3(700.0f, 209.0f, 500.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 1.0f, 1.0f),
                                 Transform(glm::vec3(700.0f, 290.0f, 300.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(0.0f, 0.0f, 1.0f),
                                 Transform(glm::vec3(50.0f, 100.0f, 250.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));*/
    /*lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 1.0f, 0.0f),
                                 Transform(glm::vec3(70.0f, 20.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));
    lights.push_back (new Light (Light::LightType::POINT,
                                 glm::vec3(1.0f, 1.0f, 0.0f),
                                 Transform(glm::vec3(70.0f, 20.0f, 50.0f),
                                           glm::vec3(0),
                                           glm::vec3(500.0f))));*/
    /*for (unsigned int i = 0; i < 50; i++) {
        float x = rand() % 1000;
        float y = rand() % 1000;
        float z = rand() % 1000;
        lights.push_back (new Light (Light::LightType::POINT,
                             glm::vec3(x/1200.0f, y/1200.0f, z/1200.0f),
                             Transform(glm::vec3(x, y, z),
                                       glm::vec3(0),
                                       glm::vec3(500.0f))));
    }
    for (unsigned int i = 0; i < lights.size(); i++) {
        int x = rand() % 1000;
        int y = rand() % 1000;
        int z = rand() % 1000;
        lights[i]->getTransform().setPosition(glm::vec3(x, y, z));
    }*/
    smokeRenderer = new ParticleRenderer<SmokeParticle, 250> (projectionMatrix,
                                                              display->getWidth(),
                                                              display->getHeight());
}

RenderingMaster::~RenderingMaster() {
    display->close();
    delete display;
    delete camera;

    delete albedoTexture;
    delete normalTexture;
    delete lightAccumulationTexture;
    delete depthTexture;
    delete blurredLightAccTexture;

    delete screenSizeRectangle;

    delete hBlurPostProcess;
    delete wBlurPostProcess;
    delete brightnessControlPostProcess;

    delete smokeRenderer;
}

void RenderingMaster::init(Display *display,
                           Camera *camera,
                           glm::mat4 projectionMatrix) {
    if (m_instance == NULL) {
        m_instance = new RenderingMaster (display, camera, projectionMatrix);
    }
}

RenderingMaster *RenderingMaster::getInstance() {
    return m_instance;
}

void RenderingMaster::destroy () {
    if (m_instance != NULL) {
        delete m_instance;
    }
}

void RenderingMaster::clearScreen(float r, float g, float b, float a, GLbitfield field) {
    display->clear (r, g, b, a, field);
}

void RenderingMaster::swapBuffers() {
    display->swapBuffers ();
}

void RenderingMaster::moveCameraForward(float distance) {
    camera->moveForward(distance);
}

void RenderingMaster::moveCameraSideways(float distance) {
    camera->moveSideways(distance);
}

void RenderingMaster::rotateCameraX(float rotX) {
    camera->rotateX(rotX);
}
void RenderingMaster::rotateCameraY(float rotY) {
    camera->rotateY(rotY);
}

Display *RenderingMaster::getDisplay() {
    return display;
}

Camera *RenderingMaster::getCamera() {
    return camera;
}

const glm::mat4 &RenderingMaster::getProjectionMatrix() {
    return projectionMatrix;
}

GBuffer &RenderingMaster::getGBuffer() {
    return gBuffer;
}

void RenderingMaster::drawDeferredShadingBuffers() {
    gBuffer.unbind();
    clearScreen(1, 1, 1, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    brightnessControlPostProcess->bind();
    brightnessControlPostProcess->process();

    hBlurPostProcess->bind();
    hBlurPostProcess->process();

    wBlurPostProcess->bind();
    wBlurPostProcess->process();

    deferredShading_BufferCombinationShader.bind();

    albedoTexture->use ();
    normalTexture->use ();
    lightAccumulationTexture->use ();
    depthTexture->use ();
    blurredLightAccTexture->use ();
    particlesRTTexture.use ();
    for (Light *light : lights) {
        if (light->getLightType() == Light::LightType::SPOT) {
            light->getDepthTextureForSpotLight().use();
            break;
        }
    }

    screenSizeRectangle->draw();

    deferredShading_BufferCombinationShader.unbind();
}

void RenderingMaster::update() {
    glm::vec3 forwardVectorInEyeSpace;
    glm::vec3 cameraForward = camera->getForward();
    glm::mat4 cameraViewMatrix;

    cameraViewMatrix = camera->getViewMatrix ();
    forwardVectorInEyeSpace = glm::mat3 (cameraViewMatrix) * cameraForward;

    deferredShading_LightAccumulationBufferCreator.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    deferredShading_LightAccumulationBufferCreator.updateUniform("cameraForwardVector", (void *) &forwardVectorInEyeSpace);

    smokeRenderer->update (*camera);
}

void RenderingMaster::computeStencilBufferForLight(Light *light) {
    gBuffer.unbind();
    gBuffer.bindForStencil();

    if (light->getLightType() == Light::LightType::POINT) {
        glDisable (GL_STENCIL_TEST);
        return;
    }

    glEnable(GL_STENCIL_TEST);
    glClear(GL_STENCIL_BUFFER_BIT);
    glStencilFunc(GL_ALWAYS, 0, 0);
    glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
    glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);

    deferredShading_StencilBufferCreator.updateUniform("viewMatrix", (void *) &camera->getViewMatrix());
    deferredShading_StencilBufferCreator.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    light->render (deferredShading_StencilBufferCreator);
}

void RenderingMaster::computeLightAccumulationBufferForLight(Light *light) {
    gBuffer.unbind();
    gBuffer.bindForLights();

    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    normalTexture->use();
    depthTexture->use();
    if (light->getLightType() == Light::LightType::SPOT) {
        bool result = true;
        light->getDepthTextureForSpotLight().use();
        result &= deferredShading_LightAccumulationBufferCreator.updateUniform("spotLightProjectionMatrix", (void *) &light->getSpotLightPerspectiveMatrix());
        result &= deferredShading_LightAccumulationBufferCreator.updateUniform("spotLightViewMatrix", (void *) &light->getSpotLightViewMatrix());
        assert (result);
    }

    light->render (deferredShading_LightAccumulationBufferCreator);
}

void RenderingMaster::createLightAccumulationBuffer() {
    gBuffer.unbind();
    gBuffer.bindForLights();
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (Light *light : lights) {
        computeStencilBufferForLight(light);
        computeLightAccumulationBufferForLight(light);
    }

    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable (GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable (GL_STENCIL_TEST);
    glDisable (GL_BLEND);
}

void RenderingMaster::beginCreateDepthTextureForSpotLight(Light *light) {
    assert (light->getLightType() == Light::LightType::SPOT);

    glCullFace(GL_FRONT);
    light->getDepthTextureFrameBufferForSpotLight().bindAllRenderTargets();

    deferredShading_StencilBufferCreator.updateUniform("viewMatrix", (void *) &light->getSpotLightViewMatrix());
    deferredShading_StencilBufferCreator.updateUniform("projectionMatrix", (void *) &light->getSpotLightPerspectiveMatrix());
}

void RenderingMaster::endCreateDepthTextureForSpotLight(Light *light) {
    assert (light->getLightType() == Light::LightType::SPOT);

    light->getDepthTextureFrameBufferForSpotLight().unbind();
    glCullFace(GL_BACK);
}

void RenderingMaster::addLightToScene (Light *light) {
    lights.push_back (light);
}

void RenderingMaster::resetLights () {
    for (Light *light : lights) {
        delete (light);
    }

    lights.clear ();
}

const std::vector <Light *> &RenderingMaster::getLights() {
    return lights;
}
