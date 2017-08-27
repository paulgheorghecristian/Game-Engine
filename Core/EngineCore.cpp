#include "EngineCore.h"

EngineCore::EngineCore(rapidjson::Document &gameDocument) : isRunning (false) {
    //create renderingmaster, physicsmaster and entities
    rapidjson::Document configDocument;
    rapidjson::ParseResult parseResult;
    std::string jsonBody;
    int screenWidth, screenHeight;
    const char *screenTitle;
    bool isFullScreen;
    float nearPlane, farPlane, fov, aspectRatio;
    Display * display = NULL;
    std::string configFile (gameDocument["ConfigFilePath"].GetString());

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

    nearPlane = (configDocument["camera"].HasMember("near")) ? configDocument["camera"]["near"].GetFloat() : 1.0f;
    farPlane = (configDocument["camera"].HasMember("far")) ? configDocument["camera"]["far"].GetFloat() : 5000.0f;
    fov = (configDocument["camera"].HasMember("fov")) ? configDocument["camera"]["fov"].GetFloat() : 75.0;
    aspectRatio = (float ) screenWidth / (float) screenHeight;

    this->mouseMoveSpeed = (configDocument["mouse"].HasMember("moveSpeed")) ? configDocument["mouse"]["moveSpeed"].GetFloat() : 0.01f;
    this->mouseRotationSpeed = (configDocument["mouse"].HasMember("rotationSpeed")) ? configDocument["mouse"]["rotationSpeed"].GetFloat() : 0.001f;
    this->warpMouse = (configDocument["mouse"].HasMember("warpMouse")) ? configDocument["mouse"]["warpMouse"].GetBool() : false;
    this->showMouse = (configDocument["mouse"].HasMember("showMouse")) ? configDocument["mouse"]["showMouse"].GetBool() : true;

    /* this needs to be called first before doing anything with OpenGL */
    display = new Display (screenWidth, screenHeight, screenTitle, isFullScreen);
    inputManager.setWarpMouse (this->warpMouse);
    SDL_ShowCursor(this->showMouse);

    if (gameDocument.HasMember("Entities")) {
        for (auto const &entity : gameDocument["Entities"].GetArray()) {
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
            }
            Transform trans(position, rotation, scale);
            new_entity = new Entity (trans);
            if (entity.HasMember("Components")) {
                for (rapidjson::Value::ConstMemberIterator itr = entity["Components"].GetObject().MemberBegin();
                        itr != entity["Components"].GetObject().MemberEnd(); ++itr) {
                    new_entity->addComponent(ComponentFactory::createComponent(itr));
                }
            }

            entities.push_back (new_entity);
        }
    }
    renderingMaster = new RenderingMaster (display,
                                           new Camera (glm::vec3(0), 0, 0, 0),
                                           glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane),
                                           entities);
}

void EngineCore::start() {
    isRunning = true;

    while (isRunning) {
        input ();
        update ();
        render ();
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

    for (auto const &entity : entities) {
        entity->input ();
    }
}

void EngineCore::render() {
    renderingMaster->render();
}

void EngineCore::update() {
    for (auto entity : entities) {
        entity->update ();
    }

    renderingMaster->update();
}

std::vector<Entity *> &EngineCore::getEntities() {
    return entities;
}

EngineCore::~EngineCore() {
    for (auto entity : entities) {
        delete entity;
    }
    delete renderingMaster;
}
