#include "EngineCore.h"

EngineCore::EngineCore(rapidjson::Document &gameDocument) : isRunning (false) {
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
    aspectRatio = (float ) screenWidth / (float) screenHeight;

    this->mouseMoveSpeed = (configDocument["mouse"].HasMember("moveSpeed")) ? configDocument["mouse"]["moveSpeed"].GetFloat() : 0.01f;
    this->mouseRotationSpeed = (configDocument["mouse"].HasMember("rotationSpeed")) ? configDocument["mouse"]["rotationSpeed"].GetFloat() : 0.001f;
    this->warpMouse = (configDocument["mouse"].HasMember("warpMouse")) ? configDocument["mouse"]["warpMouse"].GetBool() : false;
    this->showMouse = (configDocument["mouse"].HasMember("showMouse")) ? configDocument["mouse"]["showMouse"].GetBool() : true;

    /* this needs to be called first before doing anything with OpenGL */
    display = new Display (screenWidth, screenHeight, screenTitle, isFullScreen, maxFps, vSync);
    inputManager.setWarpMouse (this->warpMouse);
    SDL_ShowCursor(this->showMouse);

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
            Entity *new_entity;

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

                position.x += rand()%500;
                position.y += rand()%500;
                position.z += rand()%500;
            }
            Transform trans(position, rotation, scale);
            new_entity = new Entity (trans);
            if (entity.HasMember("Components")) {
                for (rapidjson::Value::ConstMemberIterator itr = entity["Components"].GetObject().MemberBegin();
                        itr != entity["Components"].GetObject().MemberEnd(); ++itr) {
                    Component *component = ComponentFactory::createComponent(itr);
                    new_entity->addComponent(component);
                }
            }
            entities.push_back (new_entity);
        }
    }

    Transform playerTrans (glm::vec3(0, 1000, 0), glm::vec3(0), glm::vec3(20));
    entities.push_back ((new Player (renderingMaster->getCamera(), playerTrans))
                        ->addComponent (new PhysicsComponent (PhysicsComponent::BoundingBodyType::SPHERE, glm::vec3(20), 50.0f)));

    std::cout << "Number of entities: " << entities.size() << std::endl;
}

void EngineCore::start() {
    isRunning = true;
    auto start_time = HighResolutionClock::now();
    const float frame_time = renderingMaster->getDisplay()->getFrameTimeInMs() * 1000;
    unsigned int unprocessed_time = 0;
    unsigned int frame_counter = 0;
    unsigned int FPS = 0;
    bool needToRender = false;
    //std::vector<Transform> old_state, save_state;

    ProfilingTimer renderTime("render");
    ProfilingTimer updateTime("update");
    ProfilingTimer inputTime("input");
    ProfilingTimer frameTime("frame");

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
        /*//if (needToRender) {
            float blend_factor = unprocessed_time / frame_time;
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
        //}
    }
}

void EngineCore::stop() {
    isRunning = false;
}

void EngineCore::input() {
    inputManager.update (renderingMaster->getDisplay());

    if(inputManager.getKeyDown (SDLK_ESCAPE)){
        stop ();
    }

    if (inputManager.getKey (SDLK_w)) {
        renderingMaster->moveCameraForward (this->mouseMoveSpeed);
    }

    if (inputManager.getKey (SDLK_s)) {
        renderingMaster->moveCameraForward (-this->mouseMoveSpeed);
    }

    if (inputManager.getKey (SDLK_d)) {
        renderingMaster->moveCameraSideways (this->mouseMoveSpeed);
    }

    if (inputManager.getKey (SDLK_a)) {
        renderingMaster->moveCameraSideways (-this->mouseMoveSpeed);
    }

    renderingMaster->rotateCameraX(inputManager.getMouseDelta().y * this->mouseRotationSpeed);
    renderingMaster->rotateCameraY(inputManager.getMouseDelta().x * this->mouseRotationSpeed);

    if (inputManager.getMouse(1)) {
        Transform transform (renderingMaster->getCamera()->getPosition(),
                             glm::vec3 (0),
                             glm::vec3 (20));
        entities.push_back ((new Entity(transform))->addComponent (new RenderComponent(Mesh::loadObject("res/models/cube4.obj"),
                                                                                       new Shader("res/shaders/example.json"),
                                                                                       NULL,
                                                                                       Material (glm::vec3(1, 0, 0),
                                                                                                 glm::vec3(0),
                                                                                                 glm::vec3(0),
                                                                                                 0.0f)))
                                                    ->addComponent (new PhysicsComponent(PhysicsComponent::BoundingBodyType::CUBE,
                                                                                         glm::vec3 (20),
                                                                                         20.0f)));
    }

    for (auto const &entity : entities) {
        entity->input ();
    }
}

void EngineCore::render() {
    RenderingMaster::clearScreen(1, 1, 1, 1);

    for (auto entity : entities) {
        entity->render ();
    }

    RenderingMaster::swapBuffers();

}

void EngineCore::update() {
    physicsMaster->update();

    for (auto entity : entities) {
        entity->update ();
    }
}

std::vector<Entity *> &EngineCore::getEntities() {
    return entities;
}

EngineCore::~EngineCore() {
    for (auto entity : entities) {
        delete entity;
    }
    RenderingMaster::destroy();
    PhysicsMaster::destroy();
}
