#include "RenderingMaster.h"

#include "ParticleRenderer.h"
#include "ParticleFactory.h"
#include "GUI.h"
#include "MathUtils.h"

#include "SpotLight.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "Common.h"

#include <typeinfo>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

#include <typeinfo>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

RenderingMaster *RenderingMaster::m_instance = NULL;
Shader RenderingMaster::simpleTextShader;
glm::vec3 RenderingMaster::sunLightColor, RenderingMaster::sunLightDirection;

RenderingMaster::RenderingMaster(Display *display,
                                 Camera *camera,
                                 glm::mat4 projectionMatrix) :  particleForwardRenderFramebuffer (display->getWidth()/2, display->getHeight()/2, 1),
                                                                particlesRTTexture (particleForwardRenderFramebuffer.getRenderTargets ()[0], 6),
                                                                volumetricLightFB (display->getWidth()/4, display->getHeight()/4, 1),
                                                                volumetricLightTxt (volumetricLightFB.getRenderTargets()[0], 9),
                                                                spotLightFlares(display->getWidth(), display->getHeight(), 1),
                                                                spotLightFlaresTxt(spotLightFlares.getRenderTargets()[0], 10)
{
    int albedoTextureUnit = 0, normalTextureUnit = 1, lightAccumulationTextureUnit = 2;
    int depthTextureUnit = 3, outputType = 1, blurredLightAccUnit = 4, spotLightDepthMapUnit = 5;
    bool result = true;

    this->display = display;
    this->camera = camera;
    this->projectionMatrix = projectionMatrix;

    updateDt = display->getFrameTimeInMs();

    gBuffer.generate (display->getWidth(), display->getHeight());

    deferredShading_SceneShader.construct ("res/shaders/deferredShadingSceneShader.json");
    result &= deferredShading_SceneShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert (result);

    deferredShading_InstanceRender.construct ("res/shaders/deferredShadingInstanceRender.json");
    result &= deferredShading_InstanceRender.updateUniform("projectionMatrix", (void *) &projectionMatrix);
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
    result &= SpotLight::getLightAccumulationShader().updateUniform("screenWidth", display->getWidth());
    result &= SpotLight::getLightAccumulationShader().updateUniform("screenHeight", display->getHeight());
    result &= SpotLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= SpotLight::getLightAccumulationShader().updateUniform("roughnessSampler", 3);
    assert (result);

    RenderingMaster::sunLightColor = glm::vec3(0.7, 0.3, 0.2)*0.3f;
    RenderingMaster::sunLightDirection = glm::normalize(glm::vec3(1, 0.4, -1));
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("dirLightDepthSampler", 0);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("eyeSpaceNormalSampler", 1);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("depthSampler", 2);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("lightColor", (void *) &RenderingMaster::sunLightColor);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("lightDirection", (void *) &RenderingMaster::sunLightDirection);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("screenWidth", display->getWidth());
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("screenHeight", display->getHeight());
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("projectionMatrix", (void *) &projectionMatrix);
    result &= DirectionalLight::getLightAccumulationShader().updateUniform("roughnessSampler", 3);
    assert(result);

    result &= PointLight::getLightAccumulationShader().updateUniform("eyeSpaceNormalSampler", 1);
    result &= PointLight::getLightAccumulationShader().updateUniform("depthSampler", 2);
    result &= PointLight::getLightAccumulationShader().updateUniform("screenWidth", display->getWidth());
    result &= PointLight::getLightAccumulationShader().updateUniform("screenHeight", display->getHeight());
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
    result &= volumetricLightShader.updateUniform("screenWidth", display->getWidth());
    result &= volumetricLightShader.updateUniform("screenHeight", display->getHeight());
    result &= volumetricLightShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    flareShader.construct("res/shaders/FlareShader.json");
    result &= flareShader.updateUniform("depthSampler", 0);
    result &= flareShader.updateUniform("screenWidth", display->getWidth());
    result &= flareShader.updateUniform("screenHeight", display->getHeight());
    result &= flareShader.updateUniform("projectionMatrix", (void *) &projectionMatrix);
    assert(result);

    albedoTexture = new Texture (gBuffer.getColorTexture(), albedoTextureUnit);
    normalTexture = new Texture (gBuffer.getNormalTexture(), normalTextureUnit);
    lightAccumulationTexture = new Texture (gBuffer.getLightAccumulationTexture(), lightAccumulationTextureUnit);
    depthTexture = new Texture (gBuffer.getDepthTexture(), depthTextureUnit);
    roughnessTexture = new Texture(gBuffer.getRoughnessTexture(), 12);

    hBlurPostProcess = new PostProcess (display->getWidth()/4, display->getHeight()/4,
                                        spotLightFlaresTxt.getTextureId(),
                                        "res/shaders/hBlurPostProcess.json");

    wBlurPostProcess = new PostProcess (display->getWidth()/4, display->getHeight()/4,
                                        hBlurPostProcess->getResultingTextureId(),
                                        "res/shaders/wBlurPostProcess.json");

    flarePostProcess = new PostProcess(display->getWidth()/4, display->getHeight()/4,
                                        wBlurPostProcess->getResultingTextureId(),
                                        "res/shaders/FlarePostProcess.json");
    flarePostProcess->getShader().updateUniform("lensFlareColorSampler", 1);
    lensFlareColorTxt = new Texture("res/textures/lenscolor.bmp", 1);

    screenSizeRectangle = Mesh::getRectangle();

    GUI::init (1920, 1080);
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
    currentLight = NULL;
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

    display->close();
    delete display;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void RenderingMaster::init(Display *display,
                           Camera *camera,
                           glm::mat4 projectionMatrix)
{
    if (m_instance == NULL) {
        m_instance = new RenderingMaster (display, camera, projectionMatrix);
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

    hBlurPostProcess->bind();
    hBlurPostProcess->process();

    wBlurPostProcess->bind();
    wBlurPostProcess->process();

    flarePostProcess->bind();
    flarePostProcess->process({lensFlareColorTxt});

    deferredShading_BufferCombinationShader.bind();

    albedoTexture->use();
    normalTexture->use();
    lightAccumulationTexture->use();
    depthTexture->use();
    roughnessTexture->use(12);
#if 0
    blurredLightAccTexture->use();
#endif
    particlesRTTexture.use ();
    for (Light *light : lights) {
        light->getShadowMapTexture().use(8);
        break;
    }
    volumetricLightTxt.use(9);
    flarePostProcess->getResultingTexture().use(10);
    wBlurPostProcess->getResultingTexture().use(11);

    screenSizeRectangle->draw();

    deferredShading_BufferCombinationShader.unbind();
}

void RenderingMaster::update() {
    glm::vec3 forwardVectorInEyeSpace;
    glm::vec3 cameraForward = glm::normalize(camera->getForward());
    glm::mat4 cameraViewMatrix;

    cameraViewMatrix = camera->getViewMatrix ();
    forwardVectorInEyeSpace = glm::mat3 (cameraViewMatrix) * cameraForward;

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
    deferredShading_InstanceRender.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    skyShader->updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    volumetricLightShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);
    flareShader.updateUniform("viewMatrix", (void *) &cameraViewMatrix);

    smokeRenderer->update(*camera, updateDt);

    for (Light *light : lights) {
        if (light->isCastingShadow()) {
            light->recomputeShadowMapViewMatrix();
            light->recomputeShadowMapProjectionMatrix();
        }
    }

    {
        // perform ray intersection with object
        //TODO this should be done only in editor mode
        const glm::vec3 &currWorldPos = camera->getPosition();
        const glm::vec3 &dir = getCurrWorldPosRay();

        glm::vec3 from_glm = currWorldPos+dir*10.0f;
        glm::vec3 to_glm = currWorldPos+dir*1000.0f;

        btVector3 from = btVector3(from_glm.x, from_glm.y, from_glm.z);
        btVector3 to = btVector3(to_glm.x, to_glm.y, to_glm.z);

        btCollisionWorld::AllHitsRayResultCallback rayCallback(from, to);
        PhysicsMaster::getInstance()->getWorld()->rayTest(from, to, rayCallback);

        for (int i = 0; i < rayCallback.m_hitFractions.size(); i++) {
            UserData *data = (UserData*) rayCallback.m_collisionObjects[i]->getUserPointer();

            if (data && data->type == PointerType::LIGHT) {
                Light *light = data->pointer.light;

                currentLight = light;
                break;
            }
        }

    }

    flarePostProcess->getShader().updateUniform("uGhostDispersal", data_f[0]);
    flarePostProcess->getShader().updateUniform("density", data_f[1]);
    flarePostProcess->getShader().updateUniform("exposure", data_f[2]);
    flarePostProcess->getShader().updateUniform("weight", data_f[3]);
    flarePostProcess->getShader().updateUniform("decay", data_f[4]);
    flarePostProcess->getShader().updateUniform("illuminationDecay", data_f[5]);

    //updateLastSpotLight();
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

    light->render();
}

void RenderingMaster::createLightAccumulationBuffer()
{
    gBuffer.unbind();
    gBuffer.bindForLights();
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (Light *light : lights) {
        if (light->needsStencilTest()) {
            computeStencilBufferForLight(light);
        }
        computeLightAccumulationBufferForLight(light);
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
}

void RenderingMaster::beginCreateDepthTextureForLight(Light *light)
{
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

void RenderingMaster::resetLights()
{
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

const std::vector<Light *> &RenderingMaster::getLights()
{
    return lights;
}

void RenderingMaster::drawSky()
{
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    skyDomeEntity.getComponent(Entity::Flags::RENDERABLE)->render();

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

void RenderingMaster::updateLastSpotLight()
{
    //TODO dead code
    Light *lastSpotLight = NULL;
    bool result = true;

    for (int i = lights.size()-1; i >= 0; i--) {
        if (typeid(*lights[i]) == typeid(SpotLight)) {
            lastSpotLight = lights[i];
            break;
        }
    }

    if (lastSpotLight == NULL) {
        return;
    }

#if 0
    result &= volumetricLightShader.updateUniform("volumetricLightProjectionMatrix", (void *) &lastSpotLight->getShadowMapProjectionMatrix());
    result &= volumetricLightShader.updateUniform("volumetricLightViewMatrix", (void *) &lastSpotLight->getShadowMapViewMatrix());
    result &= volumetricLightShader.updateUniform("lightColor", (void *) &lastSpotLight->getLightColor());
    result &= volumetricLightShader.updateUniform("lightPosition", (void *) &lastSpotLight->getTransform().getPosition());
    result &= volumetricLightShader.updateUniform("modelMatrix", (void *) &lastSpotLight->getTransform().getModelMatrix());
    assert(result);
#endif

    //lastSpotLight->getTransform().addRotation(0, 0.01, 0.01);
}

void RenderingMaster::renderVolumetricLight()
{
    volumetricLightFB.bindAllRenderTargets();
    for (int i = lights.size()-1; i >= 0; i--) {
        if (typeid(*lights[i]) == typeid(SpotLight)) {
            if (i != 1)
                continue;
            lights[i]->getShadowMapTexture().use(0);
            depthTexture->use(1);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc (GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            volumetricLightShader.updateUniform("volumetricLightProjectionMatrix", (void *) &lights[i]->getShadowMapProjectionMatrix());
            volumetricLightShader.updateUniform("volumetricLightViewMatrix", (void *) &lights[i]->getShadowMapViewMatrix());
            volumetricLightShader.updateUniform("lightColor", (void *) &lights[i]->getLightColor());
            volumetricLightShader.updateUniform("lightPosition", (void *) &lights[i]->getTransform().getPosition());
            volumetricLightShader.updateUniform("modelMatrix", (void *) &lights[i]->getTransform().getModelMatrix());
            volumetricLightShader.updateUniform("cameraPosition", (void *) &camera->getPosition());
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

void RenderingMaster::imguiDrawCalls() {
    {
        ImGui::Begin("GUI!");

        //ImGui::Text("This is some useful text.");
        for (int i = 0; i < GUIVarsEnum_f::NUM_VARS_f; i++) {
            std::string name("float ");
            name += i+'0';
            ImGui::SliderFloat(name.c_str(), &data_f[i], 0.0f, 10.0f);
        }

        for (int i = 0; i < GUIVarsEnum_vec3::NUM_VARS_v3; i++) {
            ImGui::ColorEdit3("clear color"+i, (float*)&data_vec3[i]);
        }

        for (int i = 0; i < GUIVarsEnum_int::NUM_VARS_i; i++) {
            if (ImGui::Button("Button"))
                data_i[i]++;

            ImGui::SameLine();
            ImGui::Text("counter_%d = %d", i, data_i[i]);
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        if (currentLight != NULL) {
            glm::vec3 &lightColor = currentLight->getLightColor();
            ImGui::ColorEdit3("light color", (float *) &lightColor);

            glm::vec3 pos = currentLight->getTransform().getPosition();

            ImGui::DragFloat("x", &pos.x, 0.05f);
            ImGui::DragFloat("y", &pos.y, 0.05f);
            ImGui::DragFloat("z", &pos.z, 0.05f);

            currentLight->getTransform().setPosition(pos);
        }
        ImGui::End();
    }
}

void RenderingMaster::renderIMGUI() {
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

glm::vec3 RenderingMaster::getWorldSpaceMouseRay(float mouseX, float mouseY) {
    float ndcX = 2.0f*mouseX / display->getWidth() - 1;
    float ndcY = 2.0f*(display->getHeight()-mouseY) / display->getHeight() - 1;

    glm::vec4 ndc(ndcX, ndcY, -1.0f, 1.0f);

    glm::vec4 clipSpace = glm::inverse(projectionMatrix) * ndc;
    glm::vec4 eyeSpace = glm::vec4(clipSpace.x, clipSpace.y, -1.0f, 0.0f);

    glm::vec4 worldSpace = glm::inverse(camera->getViewMatrix()) * eyeSpace;

    return glm::normalize(glm::vec3(worldSpace.x, worldSpace.y, worldSpace.z));
}

void RenderingMaster::computeWorldPosRay(float mouseX, float mouseY) {
    currMouseRayWorldPos = getWorldSpaceMouseRay(mouseX, mouseY);
}

const glm::vec3 &RenderingMaster::getCurrWorldPosRay() {
    return currMouseRayWorldPos;
}
