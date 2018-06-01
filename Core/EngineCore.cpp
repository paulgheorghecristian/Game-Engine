#include "EngineCore.h"

#include <glm/gtx/rotate_vector.hpp>
#include "ParticleRenderer.h"
#include "ParticleFactory.h"
#include "SpotLight.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include <chrono>
#include <thread>

EngineCore::EngineCore(rapidjson::Document &gameDocument) {
    //create renderingmaster, physicsmaster and entities
    rapidjson::Document configDocument;
    rapidjson::ParseResult parseResult;
    std::string jsonBody;
    int screenWidth, screenHeight;
    const char *screenTitle;
    bool isFullScreen, vSync;
    float nearPlane, farPlane, fov, aspectRatio;
    unsigned int maxFps;
    Display * display = NULL;
    std::string configFile (gameDocument["ConfigFilePath"].GetString());
    float gravity = gameDocument.HasMember("gravity") ? gameDocument["gravity"].GetFloat() : 9.8f;

    jsonBody = FileUtils::loadFileInString (configFile);
    parseResult = configDocument.Parse (jsonBody.c_str ());
    if (!parseResult) {
        std::cout << "Config file (" << configFile << ") parse error: " << rapidjson::GetParseError_En (parseResult.Code ()) << " (" << parseResult.Offset() << ")" << std::endl;;
        exit (-1);
    }

    if (!configDocument.HasMember ("display") ||
        !configDocument.HasMember ("camera")) {
        std::cout << "Incomplete config file: " << configFile << std::endl;
        exit (-1);
    }

    screenWidth = (configDocument["display"].HasMember("width")) ? configDocument["display"]["width"].GetInt() : 800;
    screenHeight = (configDocument["display"].HasMember("height")) ? configDocument["display"]["height"].GetInt() : 600;
    screenTitle = (configDocument["display"].HasMember("title")) ? configDocument["display"]["title"].GetString() : "No title";
    isFullScreen = (configDocument["display"].HasMember("isFullScreen")) ? configDocument["display"]["isFullScreen"].GetBool() : false;
    maxFps = (configDocument["display"].HasMember("maxFps")) ? configDocument["display"]["maxFps"].GetInt() : 60;
    vSync = (configDocument["display"].HasMember("vSync")) ? configDocument["display"]["vSync"].GetBool() : false;

    nearPlane = (configDocument["camera"].HasMember("near")) ? configDocument["camera"]["near"].GetFloat() : 1.0f;
    farPlane = (configDocument["camera"].HasMember("far")) ? configDocument["camera"]["far"].GetFloat() : 5000.0f;
    fov = (configDocument["camera"].HasMember("fov")) ? configDocument["camera"]["fov"].GetFloat() : 75.0;

    this->mouseMoveSpeed = (configDocument["mouse"].HasMember("moveSpeed")) ? configDocument["mouse"]["moveSpeed"].GetFloat() : 0.01f;
    this->mouseRotationSpeed = (configDocument["mouse"].HasMember("rotationSpeed")) ? configDocument["mouse"]["rotationSpeed"].GetFloat() : 0.001f;
    this->warpMouse = (configDocument["mouse"].HasMember("warpMouse")) ? configDocument["mouse"]["warpMouse"].GetBool() : false;
    this->showMouse = (configDocument["mouse"].HasMember("showMouse")) ? configDocument["mouse"]["showMouse"].GetBool() : true;

    /* this needs to be called first before doing anything with OpenGL */
    display = new Display (screenWidth, screenHeight, screenTitle, isFullScreen, maxFps, vSync);
    aspectRatio = (float) display->getWidth() / (float) display->getHeight();
    inputManager.setWarpMouse (this->warpMouse);
    SDL_ShowCursor(this->showMouse);

    RenderingMaster::init (display,
                           new Camera (glm::vec3(-500, 0, 0), 0, 0, 0),
                           glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane));
    PhysicsMaster::init (gravity);

    if (gameDocument.HasMember("Entities")) {
        unsigned int numberOfEntities = gameDocument["Entities"].GetArray().Size();
        unsigned int step = 0;
        for (auto const &entity : gameDocument["Entities"].GetArray()) {
            step++;
            std::cout << "Loading...%" << (float) step * 100 / numberOfEntities << std::endl;
            glm::vec3 position (0), rotation (0), scale (1);

            if (entity.HasMember("Transform")) {
                position.x = entity["Transform"]["position"].GetArray()[0].GetFloat();
                position.y = entity["Transform"]["position"].GetArray()[1].GetFloat();
                position.z = entity["Transform"]["position"].GetArray()[2].GetFloat();

                rotation.x = entity["Transform"]["rotation"].GetArray()[0].GetFloat();
                rotation.y = entity["Transform"]["rotation"].GetArray()[1].GetFloat();
                rotation.z = entity["Transform"]["rotation"].GetArray()[2].GetFloat();

                scale.x = entity["Transform"]["scale"].GetArray()[0].GetFloat();
                scale.y = entity["Transform"]["scale"].GetArray()[1].GetFloat();
                scale.z = entity["Transform"]["scale"].GetArray()[2].GetFloat();
            }
            Transform trans(position, rotation, scale);
            Entity *newEntity = new Entity ();
            newEntity->setTransform (trans);
            if (entity.HasMember("Components")) {
                for (rapidjson::Value::ConstMemberIterator itr = entity["Components"].GetObject().MemberBegin();
                        itr != entity["Components"].GetObject().MemberEnd(); ++itr) {
                    Component *component = ComponentFactory::createComponent(itr);
                    newEntity->addComponent(component);
                }
            }
            entities.push_back (newEntity);
        }
    }
    Transform floorTransform(glm::vec3(0), glm::vec3(0, 0, 0), glm::vec3(5000.0f));
    Entity *floor = new Entity();
    floor->setTransform(floorTransform);
    floor->addComponent(new RenderComponent(Mesh::getRectangleYUp(),
                                            (new Shader())->construct("res/shaders/example.json"),
                                            NULL,
                                            NULL,
                                            Material(glm::vec3(0.5f),
                                                     glm::vec3(1.0f),
                                                     glm::vec3(1.0f),
                                                     0.5f)));
    entities.push_back (floor);
    //entities.clear();
    constructPlayer();
#if 0
    Entity *debugCuboid = new Entity();
    Transform cuboidTransform(glm::vec3(0), glm::vec3(0, 0, 0), glm::vec3(1.0f));
    debugCuboid->setTransform(cuboidTransform);
    debugCuboid->addComponent(new RenderComponent(RenderingMaster::getInstance()->cuboidMesh,
                                                    (new Shader())->construct("res/shaders/example.json"),
                                                    NULL,
                                                    NULL,
                                                    Material(glm::vec3(0.0f, 0.0f, 1.0f),
                                                             glm::vec3(1.0f),
                                                             glm::vec3(1.0f),
                                                             0.5f)));
    entities.push_back (debugCuboid);

    Entity *frustumEntity = new Entity();
    Transform frustumTransform(glm::vec3(0), glm::vec3(0, 0, 0), glm::vec3(1.0f));
    frustumEntity->setTransform(frustumTransform);
    frustumEntity->addComponent(new RenderComponent(RenderingMaster::getInstance()->frustumMesh,
                                                    (new Shader())->construct("res/shaders/example.json"),
                                                    NULL,
                                                    NULL,
                                                    Material(glm::vec3(1.0f, 0.0f, 0.0f),
                                                             glm::vec3(1.0f),
                                                             glm::vec3(1.0f),
                                                             0.5f)));
    entities.push_back (frustumEntity);

    Entity *zArrow = new Entity();
    Transform zTransform(glm::vec3(0), glm::vec3(0, 0, 0), glm::vec3(10.0f));
    zArrow->setTransform(zTransform);
    zArrow->addComponent(new RenderComponent(Mesh::getArrowMesh(),
                                                    (new Shader())->construct("res/shaders/example.json"),
                                                    NULL,
                                                    NULL,
                                                    Material(glm::vec3(1.0f, 0.0f, 0.0f),
                                                             glm::vec3(1.0f),
                                                             glm::vec3(1.0f),
                                                             0.5f)));
    entities.push_back (zArrow);

    Entity *lightDir = new Entity();
    glm::vec3 lightDirVec = RenderingMaster::dirLightDirection;
    float xRot = glm::degrees(glm::asin(lightDirVec.y / glm::length(lightDirVec)));
    float yRot = glm::degrees(glm::atan(-lightDirVec.x, -lightDirVec.z));
    std::cout << "xRot=" << xRot << " " << "yRot=" << yRot << std::endl;
    Transform zTransform2(glm::vec3(0), glm::vec3(xRot, yRot, 0), glm::vec3(10.0f));
    lightDir->setTransform(zTransform2);
    lightDir->addComponent(new RenderComponent(Mesh::getArrowMesh(),
                                                    (new Shader())->construct("res/shaders/example.json"),
                                                    NULL,
                                                    NULL,
                                                    Material(glm::vec3(0.0f, 0.0f, 1.0f),
                                                             glm::vec3(1.0f),
                                                             glm::vec3(1.0f),
                                                             0.5f)));
    entities.push_back (lightDir);
#endif
    std::cout << "Number of entities: " << entities.size() << std::endl;

    outputType = 5;

    RenderingMaster::getInstance()->smokeRenderer = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone.json");
    RenderingMaster::getInstance()->smokeRenderer2 = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone2.json");
    RenderingMaster::getInstance()->addLightToScene(new DirectionalLight(Transform(glm::vec3(0),
                                                                         glm::vec3(0),
                                                                         glm::vec3(0)),
                                                                         RenderingMaster::sunLightColor,
                                                                         RenderingMaster::sunLightDirection));

    RenderingMaster::getInstance()->skyShader = new Shader("res/shaders/skyShader.json");
    RenderingMaster::getInstance()->skyDomeEntity.addComponent(new RenderComponent(Mesh::getDome(10, 10),
                                                   RenderingMaster::getInstance()->skyShader,
                                                   NULL,
                                                   NULL,
                                                   Material(glm::vec3(0), glm::vec3(0), glm::vec3(0), 0)));
    RenderingMaster::getInstance()->skyDomeEntity.setTransform(Transform(glm::vec3(0, -10, 0), glm::vec3(0), glm::vec3(100.0f)));

    fpsGUI = new BarGUI (glm::vec3(100, 100, 0),
                         glm::vec2(0, 0),
                         glm::vec3(1,0,0),
                         maxFps,
                         false,
                         glm::vec2(0),
                         "FPS");
}

void EngineCore::start() {
    isRunning = true;
    auto start_time = HighResolutionClock::now();
    const double frame_time = RenderingMaster::getInstance()->getDisplay()->getFrameTimeInMs() * 1000;
    unsigned int unprocessed_time = 0;
    unsigned int frame_counter = 0;
    unsigned int FPS = 0;
    bool needToRender = false;
    //std::vector<Transform> old_state, save_state;

    /*old_state.reserve (entities.size()*1000);
    save_state.reserve (entities.size()*1000);
    for (Entity *e : entities) {
        old_state.push_back (e->getTransform());
    }*/

    while (isRunning) {

        if (frame_counter >= 1000000) {
            #if 0
            std::cout << "----------------------" << std::endl;
            std::cout << "FPS: " << FPS << std::endl;
            std::cout << ((double) frame_counter / 1000) / FPS << " milliseconds frametime" << std::endl;
            #endif
            fpsGUI->update((int) FPS);
            //printf ("FPS=%d\n", (int) FPS);
            FPS = 0;
            frame_counter = 0;
            PT_Reset("input");
            PT_Reset("update");
            PT_Reset("render");
            PT_Reset("renderScene");
            PT_Reset("lightAcc");
            PT_Reset("computeDepthTextureForLight");
            PT_Reset("swapBuffers");
            PT_Reset("particleDraw");
            PT_Reset("buffersDraw");
            PT_Reset("GUIRender");
            #if 0
            std::cout << "----------------------" << std::endl;
            #endif
        }
        auto last_time = HighResolutionClock::now();
        unsigned int passed_time = std::chrono::duration_cast<std::chrono::microseconds>(last_time - start_time).count();

        needToRender = false;
        start_time = last_time;

        unprocessed_time += passed_time;
        frame_counter += passed_time;

        while (unprocessed_time >= frame_time) {
            /*for (unsigned int i = 0; i < entities.size(); i++) {
                //old_state[i] = entities[i]->getTransform();
            }*/
            PT_FromHere("input");
            input ();
            PT_ToHere("input");
            PT_FromHere("update");
            update ();
            PT_ToHere("update");
            unprocessed_time -= frame_time;
            needToRender = true;
        }

        if (needToRender) {
            /*float blend_factor = unprocessed_time / frame_time;
            //std::cout << blend_factor << std::endl;
            for (unsigned int i = 0; i < entities.size(); i++) {
                //save_state[i] = entities[i]->getTransform();
            }
            for (unsigned int i = 0; i < entities.size(); i++) {
                //entities[i]->getTransform().interpolateWith(old_state[i], blend_factor);
            }*/
            //unprocessed_time = 0;
            PT_FromHere ("render");
            render ();
            PT_ToHere ("render");
            /*for (unsigned int i = 0; i < entities.size(); i++) {
                //entities[i]->setTransform (save_state[i]);
            }*/
            FPS++;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void EngineCore::stop() {
    isRunning = false;
}

void EngineCore::input() {
    inputManager.update (RenderingMaster::getInstance()->getDisplay());

    if (inputManager.getKeyDown(SDLK_ESCAPE)) {
        stop ();
    }

    RenderingMaster::getInstance()->rotateCameraX(inputManager.getMouseDelta().y * this->mouseRotationSpeed);
    RenderingMaster::getInstance()->rotateCameraY(inputManager.getMouseDelta().x * this->mouseRotationSpeed);

    if (inputManager.getMouse(1)) {
        Transform transform (RenderingMaster::getInstance()->getCamera()->getPosition(),
                             glm::vec3 (0),
                             glm::vec3 (20));
        Entity *newEntity = new Entity();
        newEntity->setTransform (transform);
        entities.push_back (newEntity->addComponent (new RenderComponent(Mesh::loadObject("res/models/cube4.obj"),
                                                                                       (new Shader())->construct("res/shaders/example.json"),
                                                                                       new Texture ("res/textures/158.JPG",0),
                                                                                       new Texture ("res/textures/158_norm.JPG",1),
                                                                                       Material (glm::vec3(1, 0, 0),
                                                                                                 glm::vec3(0),
                                                                                                 glm::vec3(0),
                                                                                                 0.0f)))
                                                    ->addComponent (new PhysicsComponent(PhysicsComponent::BoundingBodyType::CUBE,
                                                                                         glm::vec3 (20),
                                                                                         20.0f)));
    }

    if (inputManager.getKeyDown(SDLK_1)) {
        outputType = 1;
    }
    if (inputManager.getKeyDown(SDLK_2)) {
        outputType = 2;
    }
    if (inputManager.getKeyDown(SDLK_3)) {
        outputType = 3;
    }
    if (inputManager.getKeyDown(SDLK_4)) {
        outputType = 4;
    }
    if (inputManager.getKeyDown(SDLK_5)) {
        outputType = 5;
    }
    if (inputManager.getKeyDown (SDLK_6)) {
        outputType = 6;
    }
    if (inputManager.getKeyDown (SDLK_7)) {
        outputType = 7;
    }
    if (inputManager.getKeyDown(SDLK_8)) {
        outputType = 8;
    }

    if (inputManager.getKeyDown (SDLK_q)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        glm::vec3 cameraRotation = RenderingMaster::getInstance()->getCamera()->getRotation();
        RenderingMaster::getInstance()->addLightToScene(new SpotLight(Transform(cameraPosition,
                                                                  glm::degrees(cameraRotation),
                                                                  glm::vec3(300.0f, 300.0f, 400.0f)),
                                                        glm::vec3(0.98f, 0.8f, 0.8f)));
    }

    if (inputManager.getKeyDown (SDLK_e)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        RenderingMaster::getInstance()->addLightToScene(new PointLight(Transform(cameraPosition,
                                                                                  glm::vec3(0),
                                                                                  glm::vec3(500.0f)),
                                                                       glm::vec3(1.0f, 0.7f, 0.2f)));

    }

    if (inputManager.getKeyDown (SDLK_r)) {
        RenderingMaster::getInstance()->resetLights();
        RenderingMaster::getInstance()->smokeRenderer->getRenderingShader().reload();

        delete RenderingMaster::getInstance()->smokeRenderer;
        delete RenderingMaster::getInstance()->smokeRenderer2;

        RenderingMaster::getInstance()->smokeRenderer = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone.json");
        RenderingMaster::getInstance()->smokeRenderer2 = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone2.json");
    }

    if (inputManager.getKeyDown (SDLK_u)) {
        RenderingMaster::getInstance()->fauxCamera->rotateX(0.09f);
    }
    if (inputManager.getKeyDown (SDLK_i)) {
        RenderingMaster::getInstance()->fauxCamera->rotateX(-0.09f);
    }
    if (inputManager.getKeyDown (SDLK_o)) {
        RenderingMaster::getInstance()->fauxCamera->rotateY(0.09f);
    }
    if (inputManager.getKeyDown (SDLK_p)) {
        RenderingMaster::getInstance()->fauxCamera->rotateY(-0.09f);
    }
    if (inputManager.getKeyDown (SDLK_k)) {
        RenderingMaster::getInstance()->fauxCamera->moveForward(0.9f);
    }
    RenderingMaster::getInstance()->deferredShading_BufferCombinationShader.updateUniform("outputType", (void *) &outputType);

    for (auto const &entity : entities) {
        entity->input (inputManager);
    }
}

void EngineCore::render() {
    /*generate deferred shading buffers*/
    PT_FromHere("renderScene");
    RenderingMaster::getInstance()->getGBuffer().bindForScene();
    RenderingMaster::getInstance()->clearScreen(1, 1, 1, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderingMaster::getInstance()->drawSky();

    for (auto entity : entities) {
        RenderComponent *renderComponent;
        if ((renderComponent =
             (RenderComponent *) (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL) {
            renderComponent->render(&RenderingMaster::getInstance()->deferredShading_SceneShader);
        }
    }
    RenderingMaster::getInstance()->getGBuffer().unbind();
    PT_ToHere("renderScene");
    /*end generate deferred shading buffers*/

    const std::vector <Light *> &lights = RenderingMaster::getInstance ()->getLights ();
    PT_FromHere("computeDepthTextureForLight");

    /* begin drawing the spot light depth map for each light*/
    for (Light *light : lights) {
        if (light->isCastingShadow()) {
            RenderingMaster::getInstance()->beginCreateDepthTextureForLight(light);
            for (auto entity : entities) {
                RenderComponent *renderComponent;
                if ((renderComponent =
                     (RenderComponent *) (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL) {
                    renderComponent->render(&RenderingMaster::getInstance()->depthMapCreator);
                }
            }
            RenderingMaster::getInstance()->endCreateDepthTextureForLight(light);
        }
    }
    /* end drawing the light depth map */
    PT_ToHere("computeDepthTextureForLight");

    PT_FromHere("lightAcc");
    RenderingMaster::getInstance()->createLightAccumulationBuffer();
    PT_ToHere("lightAcc");

    RenderingMaster::getInstance()->getGBuffer().unbind();

    PT_FromHere("particleDraw");
    RenderingMaster::getInstance()->particleForwardRenderFramebuffer.bindAllRenderTargets();
    RenderingMaster::getInstance()->depthTexture->use();
    RenderingMaster::getInstance()->smokeRenderer->draw();
    RenderingMaster::getInstance()->smokeRenderer2->draw();
    RenderingMaster::getInstance()->particleForwardRenderFramebuffer.unbind();
    PT_ToHere("particleDraw");

    PT_FromHere("buffersDraw");
    RenderingMaster::getInstance()->drawDeferredShadingBuffers();
    PT_ToHere ("buffersDraw");

    PT_FromHere("GUIRender");
    ProfilingTimer::renderAllBarGUIs();
    fpsGUI->render();
    PT_ToHere ("GUIRender");

    PT_FromHere("swapBuffers");
    RenderingMaster::getInstance()->swapBuffers();
    PT_ToHere("swapBuffers");
}

void EngineCore::update() {
    for (auto entity : entities) {
        entity->update ();
    }

    PhysicsMaster::getInstance()->update();
    RenderingMaster::getInstance()->update();

    ProfilingTimer::updateAllBarGUIs();
}

std::vector<Entity *> &EngineCore::getEntities() {
    return entities;
}

void EngineCore::constructPlayer() {
    Transform playerTrans (glm::vec3(200, 10, 0), glm::vec3(0), glm::vec3(10));
    PhysicsComponent *playerPhysicsComponent = new PhysicsComponent (PhysicsComponent::BoundingBodyType::CAPSULE,
                                                                    glm::vec3(10),
                                                                    30.0f);
    entities.push_back ((new Player (playerTrans))
                        ->addComponent (playerPhysicsComponent));
    playerPhysicsComponent->getRigidBody()->setDamping(btScalar(0.5), btScalar(0.0));
    playerPhysicsComponent->getRigidBody()->setSleepingThresholds(0.0, 0.0);
    playerPhysicsComponent->getRigidBody()->setAngularFactor(0.0);
    playerPhysicsComponent->getRigidBody()->setFriction(0.5);
    playerPhysicsComponent->getRigidBody()->setRestitution(0.6);
}

EngineCore::~EngineCore() {
    for (auto entity : entities) {
        delete entity;
    }
    RenderingMaster::destroy();
    PhysicsMaster::destroy();
}
