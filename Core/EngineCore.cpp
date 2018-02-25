#include "EngineCore.h"

EngineCore::EngineCore(rapidjson::Document &gameDocument) : isRunning (false),
                                                            renderTime("render"),
                                                            updateTime("update"),
                                                            inputTime("input"),
                                                            frameTime("frame"),
                                                            renderSceneTime("renderScene"),
                                                            lightAccumBufferTime("lightAccBuffer"),
                                                            screenDrawTime("screenDraw") {
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
    aspectRatio = (float) display->getWidth() / (float ) display->getHeight();
    inputManager.setWarpMouse (this->warpMouse);
    SDL_ShowCursor(this->showMouse);

    Light::setPointMesh(Mesh::loadObject("res/models/lightSphere.obj"));
    Light::setSpotMesh(Mesh::loadObject("res/models/SpotLightMesh5.obj"));
    RenderingMaster::init (display,
                           new Camera (glm::vec3(-500, 0, 0), 0, glm::radians(90.0f), 0),
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
    constructPlayer();
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
    std::cout << "Number of entities: " << entities.size() << std::endl;

    outputType = 5;
}

void EngineCore::start() {
    isRunning = true;
    auto start_time = HighResolutionClock::now();
    const float frame_time = RenderingMaster::getInstance()->getDisplay()->getFrameTimeInMs() * 1000;
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
            std::cout << "----------------------" << std::endl;
            std::cout << "FPS: " << FPS << std::endl;
            std::cout << (1000.0f * ((float)frame_counter / 1000000))/FPS << " milliseconds frametime" << std::endl;
            FPS = 0;
            frame_counter = 0;
            PT_PrintAndReset(inputTime);
            PT_PrintAndReset(updateTime);
            PT_PrintAndReset(renderTime);
            PT_PrintAndReset(renderSceneTime);
            PT_PrintAndReset(lightAccumBufferTime);
            PT_PrintAndReset(screenDrawTime);
            std::cout << "----------------------" << std::endl;
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
            PT_FromHere(inputTime);
            input ();
            PT_ToHere(inputTime);
            PT_FromHere(updateTime);
            update ();
            PT_ToHere(updateTime);
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
            PT_FromHere (renderTime);
            render ();
            PT_ToHere (renderTime);
            /*for (unsigned int i = 0; i < entities.size(); i++) {
                //entities[i]->setTransform (save_state[i]);
            }*/
            FPS++;
        }
    }
}

void EngineCore::stop() {
    isRunning = false;
}

void EngineCore::input() {
    inputManager.update (RenderingMaster::getInstance()->getDisplay());

    if(inputManager.getKeyDown (SDLK_ESCAPE)){
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
                                                                                       new Texture ("res/textures/196.bmp",0),
                                                                                       NULL,
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

    if (inputManager.getKeyDown (SDLK_q)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        glm::vec3 cameraRotation = RenderingMaster::getInstance()->getCamera()->getRotation();
        Light *newLight = new Light (Light::LightType::SPOT,
                                    glm::vec3(1.0f, 1.0f, 1.0f),
                                    Transform(cameraPosition,
                                              cameraRotation,
                                              glm::vec3(300.0f, 300.0f, 400.0f)));
        RenderingMaster::getInstance()->addLightToScene(newLight);
    }

    if (inputManager.getKeyDown (SDLK_e)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        RenderingMaster::getInstance()->addLightToScene(new Light (Light::LightType::POINT,
                                                    glm::vec3(1.0f, 1.0f, 1.0f),
                                                    Transform(cameraPosition,
                                                              glm::vec3(0),
                                                              glm::vec3(500.0f))));
    }
    RenderingMaster::getInstance()->deferredShading_BufferCombinationShader.updateUniform("outputType", (void *) &outputType);

    for (auto const &entity : entities) {
        entity->input (inputManager);
    }
}

void EngineCore::render() {
    /*generate deferred shading buffers*/
    PT_FromHere(renderSceneTime);
    RenderingMaster::getInstance()->getGBuffer().bindForScene();
    RenderingMaster::getInstance()->clearScreen (1, 1, 1, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (auto entity : entities) {
        RenderComponent *renderComponent;
        if ((renderComponent =
             static_cast<RenderComponent *> (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL) {
            renderComponent->render(&RenderingMaster::getInstance()->deferredShading_SceneShader);
        }
    }

    RenderingMaster::getInstance()->smokeRenderer->draw ();

    RenderingMaster::getInstance()->getGBuffer().unbind();
    PT_ToHere(renderSceneTime);
    /*end generate deferred shading buffers*/

    /* begin drawing the spot light depth map for each spot light*/
    for (Light *l : RenderingMaster::getInstance()->getLights()) {
        if (l->getLightType() == Light::LightType::SPOT) {
            RenderingMaster::getInstance()->beginCreateDepthTextureForSpotLight(l);
            for (auto entity : entities) {
                RenderComponent *renderComponent;
                if ((renderComponent =
                     static_cast<RenderComponent *> (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL) {
                    renderComponent->render(&RenderingMaster::getInstance()->deferredShading_StencilBufferCreator);
                }
            }
            RenderingMaster::getInstance()->endCreateDepthTextureForSpotLight(l);
        }
    }
    /* end drawing the spot light depth map */

    PT_FromHere(lightAccumBufferTime);
    RenderingMaster::getInstance()->createLightAccumulationBuffer();
    PT_ToHere(lightAccumBufferTime);

    PT_FromHere(screenDrawTime);
    RenderingMaster::getInstance()->getGBuffer().unbind();
    RenderingMaster::getInstance()->drawDeferredShadingBuffers();

    RenderingMaster::getInstance()->swapBuffers();
    PT_ToHere(screenDrawTime);
}

void EngineCore::update() {
    PhysicsMaster::getInstance()->update();
    RenderingMaster::getInstance()->update();

    for (auto entity : entities) {
        entity->update ();
    }
}

std::vector<Entity *> &EngineCore::getEntities() {
    return entities;
}

void EngineCore::constructPlayer() {
    Transform playerTrans (glm::vec3(0, 1000, 0), glm::vec3(0), glm::vec3(10));
    PhysicsComponent *playerPhysicsComponent = new PhysicsComponent (PhysicsComponent::BoundingBodyType::CAPSULE,
                                                                    glm::vec3(10),
                                                                    50.0f);
    entities.push_back ((new Player (RenderingMaster::getInstance()->getCamera(), playerTrans))
                        ->addComponent (playerPhysicsComponent));
    playerPhysicsComponent->getRigidBody()->setDamping(btScalar(0.1), btScalar(0.1));
    playerPhysicsComponent->getRigidBody()->setSleepingThresholds(0.0, 0.0);
    playerPhysicsComponent->getRigidBody()->setAngularFactor(0.0);
    playerPhysicsComponent->getRigidBody()->setFriction(0.7);
    playerPhysicsComponent->getRigidBody()->setRestitution(0.6);
}

EngineCore::~EngineCore() {
    for (auto entity : entities) {
        delete entity;
    }
    RenderingMaster::destroy();
    PhysicsMaster::destroy();
}
