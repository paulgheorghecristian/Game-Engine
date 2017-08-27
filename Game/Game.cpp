#include "Game.h"

Game::Game(const std::string &gameJsonFilePath) {
    rapidjson::Document gameDocument;
    rapidjson::ParseResult parseResult;
    std::string jsonBody;

    jsonBody = FileUtils::loadFileInString (gameJsonFilePath);
    parseResult = gameDocument.Parse (jsonBody.c_str ());
    if (!parseResult) {
        std::cout << "Config file (" << gameJsonFilePath << ") parse error: " << rapidjson::GetParseError_En (parseResult.Code ()) << " (" << parseResult.Offset() << ")" << std::endl;;
        exit (-1);
    }

    if (!gameDocument.HasMember ("ConfigFilePath")) {
        std::cout << "Incomplete config file: " << gameJsonFilePath << std::endl;
        exit (-1);
    }

    engineCore = new EngineCore(gameDocument);
}

void Game::start() {
    engineCore->start();
}

Game::~Game() {
    delete engineCore;
}
