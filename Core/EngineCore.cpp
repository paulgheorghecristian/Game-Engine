#include "EngineCore.h"

#include <glm/gtx/rotate_vector.hpp>
#include "ParticleRenderer.h"
#include "ParticleFactory.h"
#include "SpotLight.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "AIPlayerFollowerComponent.hpp"
#include <chrono>
#include <thread>
#include "SkeletalAnimationComponent.hpp"
#include "Common.h"
#include "TextButton.hpp"
#include "SelectionButton.hpp"
#include "SlideButton.hpp"
#include "ParticleEmitter.h"
#include "GrassRenderer.h"
#include "BlackHole.h"
#include "imgui.h"
#include "ParticleEmitterComponent.h"
#include "WaterRenderer.h"
#include "AudioMaster.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include "stb_image_write.h"
#include <cstring>
#include <ctime>
#include <sys/stat.h>

static std::string jsonToString(const rapidjson::Value &value) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}

static std::string savePathFor(const std::string &mapPath) {
    std::string base = mapPath.empty() ? std::string("Game/map") : mapPath;
    size_t dot = base.rfind(".json");
    if (dot != std::string::npos) {
        base = base.substr(0, dot);
    }
    const std::string suffix = "_saved";
    if (base.size() >= suffix.size() && base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return base + ".json"; /* saving a saved map overwrites it */
    }
    return base + suffix + ".json";
}

EngineCore::EngineCore(rapidjson::Document &gameDocument, const std::string &mapPath) {
    m_mapPath = mapPath;
    m_savePath = savePathFor(mapPath);
    //create renderingmaster, physicsmaster and entities
    rapidjson::Document configDocument;
    rapidjson::ParseResult parseResult;
    std::string jsonBody;
    int screenWidth, screenHeight;
    const char *screenTitle;
    bool isFullScreen, vSync;
    float nearPlane, farPlane, fov, aspectRatio, renderScale;
    unsigned int maxFps;
    Display * display = NULL;
    m_configFilePath = std::string(gameDocument["ConfigFilePath"].GetString());
    m_gravity = gameDocument.HasMember("gravity") ? gameDocument["gravity"].GetFloat() : 9.8f;
    secondFraction = 0;

    jsonBody = FileUtils::loadFileInString (m_configFilePath);
    parseResult = configDocument.Parse (jsonBody.c_str ());
    if (!parseResult) {
        std::cout << "Config file (" << m_configFilePath << ") parse error: " <<
        rapidjson::GetParseError_En (parseResult.Code ()) << " (" << parseResult.Offset() << ")" << std::endl;
        exit (-1);
    }

    if (!configDocument.HasMember ("display") ||
        !configDocument.HasMember ("camera")) {
        std::cout << "Incomplete config file: " << m_configFilePath << std::endl;
        exit (-1);
    }

    screenWidth = (configDocument["display"].HasMember("width")) ? configDocument["display"]["width"].GetInt() : 800;
    screenHeight = (configDocument["display"].HasMember("height")) ? configDocument["display"]["height"].GetInt() : 600;
    screenTitle = (configDocument["display"].HasMember("title")) ? configDocument["display"]["title"].GetString() : "No title";
    isFullScreen = (configDocument["display"].HasMember("isFullScreen")) ? configDocument["display"]["isFullScreen"].GetBool() : false;
    /* "updateRate": game ticks per second (physics, logic, animation).
     * "maxFps": render cap; 0 = unlimited (vsync still applies). Older configs
     * only had maxFps, which used to be the tick rate: default the tick rate to
     * it so those behave as before. */
    maxFps = (configDocument["display"].HasMember("maxFps")) ? configDocument["display"]["maxFps"].GetInt() : 0;
    unsigned int updateRate = (configDocument["display"].HasMember("updateRate")) ?
                              configDocument["display"]["updateRate"].GetInt() : (maxFps > 0 ? maxFps : 60);
    /* GE_MAX_FPS=n: another cap for this run (trying how one feels) */
    if (const char *capEnv = std::getenv("GE_MAX_FPS")) {
        maxFps = (unsigned int) std::lround(std::atof(capEnv));
    }
    m_maxRenderFps = maxFps;
    vSync = (configDocument["display"].HasMember("vSync")) ? configDocument["display"]["vSync"].GetBool() : false;
    renderScale = (configDocument["display"].HasMember("renderScale")) ? configDocument["display"]["renderScale"].GetFloat() : 1.0f;
    if (const char *rs = std::getenv("GE_RENDER_SCALE")) {
        renderScale = std::atof(rs);
    }

    nearPlane = (configDocument["camera"].HasMember("near")) ? configDocument["camera"]["near"].GetFloat() : 1.0f;
    farPlane = (configDocument["camera"].HasMember("far")) ? configDocument["camera"]["far"].GetFloat() : 5000.0f;
    fov = (configDocument["camera"].HasMember("fov")) ? configDocument["camera"]["fov"].GetFloat() : 75.0;

    this->mouseMoveSpeed = (configDocument["mouse"].HasMember("moveSpeed")) ? configDocument["mouse"]["moveSpeed"].GetFloat() : 0.01f;
    this->mouseRotationSpeed = (configDocument["mouse"].HasMember("rotationSpeed")) ? configDocument["mouse"]["rotationSpeed"].GetFloat() : 0.001f;
    this->warpMouse = (configDocument["mouse"].HasMember("warpMouse")) ? configDocument["mouse"]["warpMouse"].GetBool() : false;
    this->showMouse = (configDocument["mouse"].HasMember("showMouse")) ? configDocument["mouse"]["showMouse"].GetBool() : true;

    /* Benchmark mode, e.g.: GE_BENCH=600 GE_SIZE=1920x1080 GE_SCREENSHOT=shot.png ./ge */
    if (const char *bench = std::getenv("GE_BENCH")) {
        m_benchFrames = std::max(1, std::atoi(bench));
        isFullScreen = std::getenv("GE_FULLSCREEN") != NULL;
        vSync = false;
        this->warpMouse = false;
        if (const char *size = std::getenv("GE_SIZE")) {
            std::sscanf(size, "%dx%d", &screenWidth, &screenHeight);
        }
        if (const char *shot = std::getenv("GE_SCREENSHOT")) {
            m_screenshotPath = shot;
        }
    }

    /* GE_REMOTE=<port>: driven from tools/timeline_editor.py (GE_SIZE: its picture) */
    const char *remotePort = std::getenv("GE_REMOTE");
    if (remotePort != NULL && m_benchFrames == 0) {
        if (const char *size = std::getenv("GE_SIZE")) {
            std::sscanf(size, "%dx%d", &screenWidth, &screenHeight);
        }
        isFullScreen = false;
        this->warpMouse = false;
    }

    /* this needs to be called first before doing anything with OpenGL */
    m_configuredVSync = vSync;
    display = new Display (screenWidth, screenHeight, screenTitle, isFullScreen, updateRate, vSync);
    /* with vsync, a cap near a whole share of the screen's refreshes is made exactly that (82 on 165 Hz:
     * 82.5, every second refresh): a cap a little off it drifts against the screen, and every so often
     * a frame waits a refresh more (a hitch every few seconds) */
    if (vSync && m_maxRenderFps > 0.0) {
        double refresh = display->getRefreshRate();
        double share = std::max(1.0, std::round(refresh / m_maxRenderFps));
        if (std::abs(refresh / share - m_maxRenderFps) <= m_maxRenderFps * 0.05) {
            m_maxRenderFps = refresh / share;
        }
        printf("Frame cap %.2f fps (every %d of %d refreshes)\n", m_maxRenderFps, (int) share, (int) refresh);
    }
    m_configuredFps = m_maxRenderFps;
    aspectRatio = (float) display->getWidth() / (float) display->getHeight();
    inputManager.setWarpMouse (this->warpMouse);
    SDL_ShowCursor(this->showMouse);

    DebugRenderer::init();
    PhysicsMaster::init(m_gravity);
    RenderingMaster::init(display,
                            new Camera (glm::vec3(-500, 0, 0), 0, 0, 0),
                            glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane),
                            renderScale);

    Light::s_tickSeconds = display->getFrameTimeInMs() / 1000.0f;
    if (gameDocument.HasMember("Water")) {
        m_waterJson = jsonToString(gameDocument["Water"]);
    }
    if (gameDocument.HasMember("Waking")) {
        m_wakingJson = jsonToString(gameDocument["Waking"]);
    }
    if (gameDocument.HasMember("RoomAlive")) {
        m_roomAliveJson = jsonToString(gameDocument["RoomAlive"]);
    }
    if (gameDocument.HasMember("Grass")) {
        m_grassJson = jsonToString(gameDocument["Grass"]);
    }
    if (gameDocument.HasMember("ParticleEmitters")) {
        m_emittersJson = jsonToString(gameDocument["ParticleEmitters"]);
    }
    if (gameDocument.HasMember("Audio")) {
        m_audioJson = jsonToString(gameDocument["Audio"]);
    }
    if (gameDocument.HasMember("Footsteps")) {
        m_footstepsJson = jsonToString(gameDocument["Footsteps"]);
    }
    if (gameDocument.HasMember("Sequences")) {
        m_sequencesJson = jsonToString(gameDocument["Sequences"]);
    }
    RenderingMaster::getInstance()->savedMapPath = m_savePath;
    RenderingMaster::getInstance()->onSaveMap = [this]() { saveMap(m_savePath); };
    RenderingMaster::getInstance()->onReloadMap = [this]() { reloadMap(m_savePath); };
    loadMapFeatures(gameDocument);
    loadEntities(gameDocument);
    loadGrass(gameDocument);
    PhysicsMaster::getInstance()->startGraphCreation();
    PhysicsMaster::getInstance()->endGraphCreation();
    // DEBUG
    // temporary debugging code
    // PhysicsMaster::getInstance()->getQuadTree().getGraph().printGraph();
    // std::unordered_map<std::size_t, btGhostObject *> quadTreeBodies = PhysicsMaster::getInstance()->getQuadTreeBodies();
    // for (auto it: quadTreeBodies) {
    //     btGhostObject *body = it.second;

    //     UserData *uData = (UserData *) body->getUserPointer();
    //     QuadTree::Node *node = (QuadTree::Node *) uData->pointer.node;
    //     btVector3 bScale = dynamic_cast<const btBoxShape* >(body->getCollisionShape())->getImplicitShapeDimensions();
    //     btVector3 bOrigin = body->getWorldTransform().getOrigin();

    //     Transform transform(glm::vec3(bOrigin.x(), bOrigin.y(), bOrigin.z()),
    //                             glm::vec3(0),
    //                             glm::vec3(bScale.x()*2.0f, bScale.y()*2.0f, bScale.z()*2.0f));
    //     Entity *newEntity = new Entity();
    //     newEntity->setTransform(transform);
    //     RenderingObject obj = RenderingObject::loadObject("res/models/cube4.obj", true, false);
    //     Material *mat = new Material();
    //     if (node->m_data.isBlocked == false) {
    //         mat->setDiffuse(glm::vec3(1.0, 0.0, 1.0));
    //     } else {
    //         mat->setDiffuse(glm::vec3(1.0, 1.0, 0.0));
    //     }

    //     assert(node->m_idx == it.first);

    //     obj.addMaterial(mat);

    //     newEntity = newEntity->addComponent(new RenderComponent(std::move(obj)));
    //     entities.push_back(newEntity);
    //     m_quadTreeEntities[node->m_idx] = newEntity;
    // }
    // END DEBUG
    {
        std::vector<Light *> &sceneLights = RenderingMaster::getInstance()->getLights();
        size_t firstMapLight = sceneLights.size();
        loadLights(gameDocument);
        /* "beam" particle emitters linked to a map light by index */
        for (ParticleEmitter *emitter : RenderingMaster::getInstance()->particleEmitters) {
            int index = emitter->getLightIndex();
            if (index >= 0 && firstMapLight + index < sceneLights.size()) {
                SpotLight *spot = dynamic_cast<SpotLight *>(sceneLights[firstMapLight + index]);
                if (spot != NULL) {
                    emitter->linkLight(spot);
                } else {
                    std::cout << "ParticleEmitter: light " << index << " is not a spot light" << std::endl;
                }
            }
        }
    }

    constructPlayer();
    // constructEnemy();

    outputType = 5;
    /* GE_OUTPUT_TYPE: start in a debug view (same as keys 1/2/4/5/6/8/0) */
    if (const char *ot = std::getenv("GE_OUTPUT_TYPE")) {
        outputType = std::atoi(ot);
    }

    /* maps with their own "ParticleEmitters" replace the default smoke cone */
    if (!gameDocument.HasMember("ParticleEmitters")) {
        RenderingMaster::getInstance()->smokeRenderer = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone.json");
    }
    RenderingMaster::getInstance()->skyDomeEntity.addComponent(new RenderComponent(RenderingObject::loadObject("__dome__")));
    RenderingMaster::getInstance()->skyDomeEntity.setTransform(Transform(glm::vec3(0, -10, 0), glm::vec3(0), glm::vec3(100.0f)));

    fpsGUI = new BarGUI (glm::vec3(100, 100, 0),
                         glm::vec2(0, 0),
                         glm::vec3(1,0,0),
                         maxFps > 0 ? maxFps : 240,
                         false,
                         glm::vec2(0),
                         "FPS");

    startMenu = new Menu("res/textures/enemy2.png", screenWidth, screenHeight);
    startMenu->addButton(new TextButton(150, 300, "New Game",
                                        glm::vec3(1.0, 1.0, 1.0),
                                        [](Button *button) {},
                                        [&](Button *button) { isOnStartMenu = false;},
                                        [](Button *button) {
                                            TextButton *bt = (TextButton *) button;
                                            bt->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            bt->setFontIndex(1);
                                        },
                                        [](Button *button) {
                                            TextButton *bt = (TextButton *) button;
                                            bt->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            bt->setFontIndex(0);
                                        }
                        ));
    startMenu->addButton(new TextButton(150, 250, "Load Game",
                                        glm::vec3(1.0, 1.0, 1.0),
                                        [](Button *button) {},
                                        [&](Button *button) {},
                                        [](Button *button) {
                                            TextButton *bt = (TextButton *) button;
                                            bt->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            bt->setFontIndex(1);
                                        },
                                        [](Button *button) {
                                            TextButton *bt = (TextButton *) button;
                                            bt->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            bt->setFontIndex(0);
                                        }
                        ));
    startMenu->addButton(new TextButton(150, 200, "Options",
                                    glm::vec3(1.0, 1.0, 1.0),
                                    [](Button *button) {},
                                    [&](Button *button) { currentMenu = optionsMenu; },
                                    [](Button *button) {
                                        TextButton *bt = (TextButton *) button;
                                        bt->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                        bt->setFontIndex(1);
                                    },
                                    [](Button *button) {
                                        TextButton *bt = (TextButton *) button;
                                        bt->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                        bt->setFontIndex(0);
                                    }
                    ));
    startMenu->addButton(new TextButton(150, 150, "Exit",
                                    glm::vec3(1.0, 1.0, 1.0),
                                    [](Button *button) {},
                                    [](Button *button) { exit(0); },
                                    [](Button *button) {
                                        TextButton *bt = (TextButton *) button;
                                        bt->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                        bt->setFontIndex(1);
                                    },
                                    [](Button *button) {
                                        TextButton *bt = (TextButton *) button;
                                        bt->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                        bt->setFontIndex(0);
                                    }
                    ));

    optionsMenu = new Menu("res/textures/enemy.png", screenWidth, screenHeight);
    optionsMenu->addButton(new SelectionButton(150, 580,
                                                "Postprocess",
                                                glm::vec3(1.0, 1.0, 1.0),
                                                {"LOW", "MEDIUM", "HIGH"}));
    optionsMenu->addButton(new SelectionButton(150, 490,
                                                "Shadows",
                                                glm::vec3(1.0, 1.0, 1.0),
                                                {"LOW", "MEDIUM", "HIGH"}));
    optionsMenu->addButton(new SelectionButton(150, 400,
                                                "Volumetrics",
                                                glm::vec3(1.0, 1.0, 1.0),
                                                {"LOW", "MEDIUM", "HIGH"}));
    optionsMenu->addButton(new SlideButton(150, 330, 0.7, "Music",
                                        glm::vec3(1.0, 1.0, 1.0),
                                        [](Button *button) {
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            slideB->setFontIndex(0);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            slideB->setFontIndex(1);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            slideB->setFontIndex(0);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            const glm::vec2 &mousePos = slideB->getCurrentMousePos();
                                            const Transform &barTransform = slideB->getBarTransform();
                                            glm::vec3 currPos = slideB->getSliderTransform().getPosition();

                                            float len = mousePos.x - currPos.x;
                                            currPos.x += len * 0.5f;

                                            slideB->getSliderTransform().setPosition(currPos);
                                            slideB->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            slideB->setFontIndex(1);
                                        }
                        ));
    optionsMenu->addButton(new SlideButton(150, 260, 0.7, "Sound FX",
                                        glm::vec3(1.0, 1.0, 1.0),
                                        [](Button *button) {
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            slideB->setFontIndex(0);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            slideB->setFontIndex(1);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            slideB->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            slideB->setFontIndex(0);
                                        },
                                        [](Button *button) {
                                            SlideButton *slideB = (SlideButton *) button;
                                            const glm::vec2 &mousePos = slideB->getCurrentMousePos();
                                            const Transform &barTransform = slideB->getBarTransform();
                                            glm::vec3 currPos = slideB->getSliderTransform().getPosition();

                                            float len = mousePos.x - currPos.x;
                                            currPos.x += len * 0.5f;

                                            slideB->getSliderTransform().setPosition(currPos);
                                            slideB->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            slideB->setFontIndex(1);
                                        }
                        ));
    optionsMenu->addButton(new TextButton(150, 100, "Back",
                                        glm::vec3(1.0, 1.0, 1.0),
                                        [](Button *button) {},
                                        [&](Button *button) { currentMenu = startMenu; },
                                        [](Button *button) {
                                            ((TextButton *) button)->getText().setColor(glm::vec3(0.5, 0.5, 0.5));
                                            ((TextButton *) button)->getText().setFontIndex(1);
                                        },
                                        [](Button *button) {
                                            ((TextButton *) button)->getText().setColor(glm::vec3(1.0, 1.0, 1.0));
                                            ((TextButton *) button)->getText().setFontIndex(0);
                                        }
                        ));
    currentMenu = startMenu;
}

void EngineCore::start() {
    if (m_benchFrames > 0) {
        runBenchmark(m_benchFrames);
        return;
    }

    /* Fixed-timestep game loop with decoupled rendering:
     *  - input/update run at exactly updateRate ticks per second (time is
     *    accumulated in double-precision milliseconds)
     *  - every loop iteration renders a frame, blending the world between the
     *    previous and the current tick (alpha), so motion is smooth at any
     *    frame rate (e.g. 165 Hz) instead of being capped to the tick rate
     *  - events are polled and mouse look is applied every frame */
    Display *display = RenderingMaster::getInstance()->getDisplay();
    const double frame_time = display->getFrameTimeInMs();
    const int maxUpdatesPerFrame = 5; /* avoid a death spiral after a long hitch */
    double unprocessed_time = 0;
    double frame_counter = 0;
    unsigned int FPS = 0, UPS = 0;
    const bool printFps = std::getenv("GE_PRINT_FPS") != NULL;
    /* GE_PACING=1: frame pacing, logged (nothing changed in how the game runs). Once a second: how
     * evenly the frames started (shortest, longest, how many late), how many frames had no game tick
     * or two (the ticks and the frames beat against each other). Every hiccup as it happens: a frame
     * whose work (CPU) or drawing (GPU, timed by the GPU) took longer than a frame's time, or that
     * started late, with when and what took the time.
     * GE_PACING=present: also waits for each frame to be shown and counts the screen's refreshes each
     * stayed up (the waiting changes the timing a little).
     * GE_PACING=walk: walks forward by itself (W held), and how evenly the camera moved frame to frame
     * (the shortest and longest step against the average: 1.00/1.00 is perfectly smooth).
     * GE_PACING=look: turns round and looks up and down by itself (all of the place comes into view:
     * a hiccup the first time something is seen) */
    const char *pacingEnv = std::getenv("GE_PACING");
    const std::string pacingMode = pacingEnv ? pacingEnv : "";
    const bool printPacing = pacingEnv != NULL;
    const bool pacingWalk = pacingMode == "walk";
    const bool pacingLook = pacingMode == "look";
    const bool pacingPresent = pacingMode == "present";
    /* (GE_BENCH_SYNC=1 here too: the logged phases include their GPU time) */
    if (printPacing && std::getenv("GE_BENCH_SYNC") != NULL) ProfilingTimer::syncGPU = true;
    /* GE_PACING_FRAMES=dir: every frame saved there as it is shown (frame_00000.jpg...): a recording
     * of the run to look through, frame by frame */
    const char *pacingFramesDir = printPacing ? std::getenv("GE_PACING_FRAMES") : NULL;
    std::vector<unsigned char> pacingPixels;
    glm::vec3 lastDrawn(0.0f);
    std::vector<float> steps;
    const auto pacingStart = HighResolutionClock::now();
    if (pacingWalk || pacingLook) {
        inputManager.setScripted(true);    /* the keys and the mouse come from here */
    }
    /* the GPU's time for each frame: a timer query per frame, read a few frames later (no waiting) */
    const int GPU_QUERIES = 4;
    GLuint gpuQueries[GPU_QUERIES] = {0};
    bool gpuQueryUsed[GPU_QUERIES] = {false};
    double gpuQueryAt[GPU_QUERIES] = {0.0};
    unsigned long pacingFrame = 0;
    if (printPacing) {
        glGenQueries(GPU_QUERIES, gpuQueries);
    }
    auto lastFrameStart = HighResolutionClock::now();
    double pacingMin = 1e9, pacingMax = 0.0;
    int pacingLong = 0, pacingNoTick = 0, pacingTwoTicks = 0;
    /* and how many of the screen's refreshes each frame stayed up: in this mode each frame waits for
     * its swap to finish (glFinish), so the time between finished swaps is the time it was shown */
    const double refreshMs = 1000.0 / std::max(1, display->getRefreshRate());
    auto lastShownAt = HighResolutionClock::now();
    bool shownBefore = false;
    int shownFor[6] = {0};

    /* the game starts with the GUI closed (F1 opens it); driven by the
     * timeline editor it leaves the mouse alone */
    inputManager.setWarpMouse(this->warpMouse);
    if (const char *remotePort = std::getenv("GE_REMOTE")) {
        if (m_remote.connect(std::atoi(remotePort))) {
            inputManager.setIgnoreUser(true);      /* the editor drives it, nothing else */
            /* what a timeline can hold, for the editor to build itself from */
            m_remote.sendParameters("{\"parameters\": " + Aftermath::parametersJson() + ", \"anchors\": " +
                                    Aftermath::anchorsJson() + ", \"sound\": " + Timeline::soundsJson() + "}");
        }
    } else {
        setPlayMode(true);
    }

    isRunning = true;
    auto start_time = HighResolutionClock::now();
    /* with a frame cap: when the next frame is due (a fixed timeline, a budget apart) */
    auto due = start_time;
    bool onSchedule = false;

    while (isRunning && !Display::isWindowClosed) {
        auto frameStart = HighResolutionClock::now();

        if (frame_counter >= 1000) {
            fpsGUI->update((int) FPS);
            if (printFps) {
                printf("FPS=%u updates/s=%u lights=%zu\n", FPS, UPS,
                       RenderingMaster::getInstance()->getLights().size());
                fflush(stdout);
            }
            if (printPacing) {
                if (!steps.empty()) {
                    float sum = 0.0f, low = 1e9f, high = 0.0f;
                    for (float step : steps) { sum += step; low = std::min(low, step); high = std::max(high, step); }
                    float mean = sum / steps.size();
                    if (mean > 1e-4f) printf("WALK step min/avg=%.2f max/avg=%.2f (avg %.3f units)\n", low / mean, high / mean, mean);
                    steps.clear();
                }
                printf("PACING frames=%u ticks=%u started every %.2f-%.2fms, late(>1.25x)=%d noTick=%d twoTicks=%d",
                       FPS, UPS, pacingMin, pacingMax, pacingLong, pacingNoTick, pacingTwoTicks);
                if (pacingPresent) {
                    printf(" | shown for 0/1/2/3/4/5+ refreshes: %d/%d/%d/%d/%d/%d",
                           shownFor[0], shownFor[1], shownFor[2], shownFor[3], shownFor[4], shownFor[5]);
                }
                printf("\n");
                for (int &count : shownFor) count = 0;
                fflush(stdout);
                pacingMin = 1e9; pacingMax = 0.0;
                pacingLong = pacingNoTick = pacingTwoTicks = 0;
            }
            FPS = 0;
            UPS = 0;
            frame_counter -= 1000;
            ProfilingTimer::resetAll();
            secondFraction = 0;
        }

        /* the game's clock moves by the time between frames. Capped and on schedule, each frame is shown
         * a budget after the one before (the screen's refreshes), even when it starts a millisecond or two
         * late or early (a sleep running long, a swap waiting): moved by the measured start instead,
         * the camera stepped 10-20% short or long on those frames, a judder while walking with every
         * frame on time. So: the time it was due */
        auto frameClock = onSchedule ? due : frameStart;
        double passed_time = std::chrono::duration<double, std::milli>(frameClock - start_time).count();
        start_time = frameClock;

        unprocessed_time += passed_time;
        frame_counter += passed_time;
        secondFraction += (float) (passed_time / 1000.0);

        PT_FromHere("pollEvents");
        inputManager.pollEvents(display);
        PT_ToHere("pollEvents");
        applyMouseLook();

        int updates = 0;
        while (unprocessed_time >= frame_time && updates < maxUpdatesPerFrame) {
            capturePreviousState();
            PT_FromHere("input");
            input ();
            PT_ToHere("input");
            PT_FromHere("update");
            update ();
            PT_ToHere("update");
            inputManager.clearEdges();
            unprocessed_time -= frame_time;
            updates++;
            UPS++;
        }
        if (printPacing) {
            /* the clock above is the schedule's; this is when the frame really started */
            double startedMs = std::chrono::duration<double, std::milli>(frameStart - lastFrameStart).count();
            lastFrameStart = frameStart;
            double budget = m_maxRenderFps > 0 ? 1000.0 / m_maxRenderFps : frame_time;
            pacingMin = std::min(pacingMin, startedMs);
            pacingMax = std::max(pacingMax, startedMs);
            pacingLong += startedMs > budget * 1.25;
            if (pacingFrame > 2 && startedMs > budget * 1.4) {
                printf("LATE start %.1fms after the last (a frame is %.1fms), %.2fs in\n", startedMs, budget,
                       std::chrono::duration<double>(frameStart - pacingStart).count());
                fflush(stdout);
            }
            pacingNoTick += updates == 0;
            pacingTwoTicks += updates >= 2;
        }
        if (updates == maxUpdatesPerFrame && unprocessed_time >= frame_time) {
            unprocessed_time = 0; /* drop the backlog instead of falling further behind */
        }

        /* the timeline editor: its commands; every so often a frame read back for it */
        bool remoteFrame = false;
        if (m_remote.connected()) {
            remoteCommands();
            double now = std::chrono::duration<double, std::milli>(frameStart.time_since_epoch()).count();
            /* paused: the world stands still too, a moment after the last jump or edit (time
             * for what that changed to show: particles, the eye) */
            if (m_remoteTouched) {
                m_remoteTouched = false;
                m_remoteSettleUntil = now + 1200.0;
            }
            RenderingMaster::getInstance()->setWorldPaused(m_remotePaused && now >= m_remoteSettleUntil);
            if (now >= m_remoteNextFrame) {
                /* the next one a frame's time after this one was due (not after now: that
                 * loses part of a frame every time and gives 25 a second for 30), without a
                 * burst to catch up after a stall */
                m_remoteNextFrame = std::max(m_remoteNextFrame + m_remoteFrameMs, now - m_remoteFrameMs);
                m_remotePixels.resize((size_t) display->getWidth() * display->getHeight() * 4);
                m_captureBeforeSwap = &m_remotePixels;
                remoteFrame = true;
            }
            /* its state: just before every frame (the editor shows them together), and in between */
            if (remoteFrame || now >= m_remoteNextState) {
                m_remoteNextState = now + 200.0;
                /* the scene's clock: seconds since the start */
                float t = m_timeline.clock();
                Camera *eye = RenderingMaster::getInstance()->getCamera();
                glm::vec3 at = eye->getPosition(), turned = glm::degrees(eye->getRotation());
                char numbers[240];
                std::snprintf(numbers, sizeof(numbers),
                              "{\"t\": %.3f, \"running\": %s, \"paused\": %s, \"volume\": %.2f, \"camera\": [%.1f, %.1f, %.1f, %.1f, %.1f], ",
                              t, m_timeline.running() ? "true" : "false", m_remotePaused ? "true" : "false",
                              AudioMaster::getInstance()->getMasterVolume(), at.x, at.y, at.z, turned.y, turned.x);
                m_remote.sendState(std::string(numbers) + "\"sounds\": \"" + AudioMaster::getInstance()->playing() + "\", " +
                                   Aftermath::timelineJson(m_timeline) + ", " + m_aftermath.appliedJson() + ", " +
                                   RenderingMaster::getInstance()->stormState() + "}");
            }
        }

        if (m_pacingModeChanged) {
            m_pacingModeChanged = false;
            applyPacingMode();
            onSchedule = false;
        }
        int gpuQuery = (int) (pacingFrame % GPU_QUERIES);
        if (printPacing) {
            /* the oldest query's answer (from GPU_QUERIES frames ago), then reuse it for this frame */
            if (gpuQueryUsed[gpuQuery]) {
                GLuint64 ns = 0;
                PT_FromHere("gpuTimerRead");
                glGetQueryObjectui64v(gpuQueries[gpuQuery], GL_QUERY_RESULT, &ns);
                PT_ToHere("gpuTimerRead");
                double gpuMs = ns / 1e6;
                double budget = m_maxRenderFps > 0 ? 1000.0 / m_maxRenderFps : frame_time;
                if (pacingFrame > GPU_QUERIES + 2 && gpuMs > budget * 0.9) {
                    printf("GPU frame %.1fms (a frame is %.1fms), %.2fs in\n", gpuMs, budget, gpuQueryAt[gpuQuery]);
                    fflush(stdout);
                }
            }
            gpuQueryAt[gpuQuery] = std::chrono::duration<double>(frameStart - pacingStart).count();
            glBeginQuery(GL_TIME_ELAPSED, gpuQueries[gpuQuery]);
        }
        if (pacingFramesDir != NULL && m_captureBeforeSwap == NULL) {
            pacingPixels.resize((size_t) display->getWidth() * display->getHeight() * 4);
            m_captureBeforeSwap = &pacingPixels;
        }
        PT_FromHere ("render");
        render ((float) (unprocessed_time / frame_time));
        PT_ToHere ("render");
        if (pacingFramesDir != NULL && m_captureBeforeSwap == &pacingPixels) {
            m_captureBeforeSwap = NULL;
            char name[64];
            std::snprintf(name, sizeof(name), "/frame_%05lu.jpg", pacingFrame);
            stbi_flip_vertically_on_write(1);
            stbi_write_jpg((std::string(pacingFramesDir) + name).c_str(), display->getWidth(), display->getHeight(), 4,
                           pacingPixels.data(), 88);
            stbi_flip_vertically_on_write(0);
        }
        if (printPacing) {
            glEndQuery(GL_TIME_ELAPSED);
            gpuQueryUsed[gpuQuery] = true;
            pacingFrame++;
        }
        if (remoteFrame) {
            m_captureBeforeSwap = NULL;
            m_remote.sendFrame(display->getWidth(), display->getHeight(), m_remotePixels.data());
        }
        FPS++;
        if (m_waitEachFrame) {
            glFinish();
        }
        if (printPacing) {
            /* a frame whose work alone ran long: what took the time */
            double workMs = std::chrono::duration<double, std::milli>(HighResolutionClock::now() - frameStart).count();
            double budget = m_maxRenderFps > 0 ? 1000.0 / m_maxRenderFps : frame_time;
            if (workMs > budget * 1.25) {
                printf("LONG frame %.1fms (%d ticks), %.2fs in:\n", workMs, updates,
                       std::chrono::duration<double>(frameStart - pacingStart).count());
                ProfilingTimer::printFrame(1.0);
                fflush(stdout);
            }
            ProfilingTimer::nextFrame();
        }
        if (pacingLook) {
            /* as a player looking round with the mouse: swinging 70 degrees each way and back (what is
             * ahead goes to each edge of the screen and past it), a little up and down. By the frame,
             * not the clock: the same sweep however slow the frames (saving them) */
            const double SWING = glm::radians(70.0), TILT = glm::radians(8.0), FRAMES = 420.0;
            double a = glm::two_pi<double>() * pacingFrame / FRAMES, b = glm::two_pi<double>() * (pacingFrame + 1) / FRAMES;
            float yaw = (float) (SWING * (std::sin(b) - std::sin(a)));
            float pitch = (float) (TILT * (std::sin(2.0 * b) - std::sin(2.0 * a)));
            inputManager.injectMouseDelta(glm::vec2(-yaw, -pitch) / this->mouseRotationSpeed);
        }
        if (pacingWalk) {
            inputManager.injectKey(SDLK_w, true);
            if (FPS > 1) steps.push_back(glm::length(m_drawnCameraPosition - lastDrawn));
            lastDrawn = m_drawnCameraPosition;
        }
        if (pacingPresent) {
            glFinish();
            auto shownAt = HighResolutionClock::now();
            if (shownBefore) {
                double ms = std::chrono::duration<double, std::milli>(shownAt - lastShownAt).count();
                shownFor[std::min(5, std::max(0, (int) std::lround(ms / refreshMs)))]++;
            }
            lastShownAt = shownAt;
            shownBefore = true;
        }

        if (m_maxRenderFps > 0) {
            /* frames on a fixed timeline (each one due a budget after the one before), not a budget
             * slept after each frame: those sleeps run a little long, and the frames drifted to
             * 54 a second on a 55 cap, an uneven step now and then. Fallen far behind (a slow frame,
             * a load): start the timeline again from now */
            auto budget = std::chrono::duration_cast<HighResolutionClock::duration>(
                std::chrono::duration<double, std::milli>(1000.0 / m_maxRenderFps));
            due += budget;
            auto now = HighResolutionClock::now();
            onSchedule = now <= due + budget;
            if (!onSchedule) {
                due = now;
            } else if (due > now) {
                std::this_thread::sleep_until(due);
            }
        }
    }
}

void EngineCore::throwFireball() {
    static const char *fireJson =
        "{\"type\":\"spray\",\"texture\":\"res/textures/glowWhite.png\",\"count\":120,\"lifetimeMs\":1100,"
        "\"radius\":2.0,\"direction\":[0,1,0],\"spread\":0.7,\"speed\":[4,14],\"gravity\":12,"
        "\"startScale\":6,\"growth\":-4,\"blend\":\"additive\",\"tint\":[1.0,0.42,0.1]}";
    const float maxFireballs = 12, lifetimeSeconds = 20.0f;

    Camera *camera = RenderingMaster::getInstance()->getCamera();
    glm::vec3 forward = glm::normalize(camera->getForward());
    glm::vec3 position = camera->getPosition() + forward * 12.0f;

    RenderingObject sphere = RenderingObject::loadObject("res/models/sphere.obj");
    for (Material *material : sphere.getMaterials()) {
        material->setDiffuse(glm::vec3(1.0f, 0.45f, 0.12f));
        material->setEmissive(1.0f);
    }
    RenderComponent *renderComponent = new RenderComponent(std::move(sphere));
    renderComponent->setCastsShadow(false);

    rapidjson::Document fire;
    fire.Parse(fireJson);

    Entity *fireball = new Entity();
    fireball->setTransform(Transform(position, glm::vec3(0), glm::vec3(2.5f)));
    PhysicsComponent *physics = new PhysicsComponent(PhysicsComponent::BoundingBodyType::SPHERE, glm::vec3(5.0f), 8.0f);
    fireball->addComponent(renderComponent)
            ->addComponent(physics)
            ->addComponent(new ParticleEmitterComponent(fire));
    glm::vec3 velocity = forward * 140.0f + glm::vec3(0, 25, 0);
    physics->getRigidBody()->setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
    physics->getRigidBody()->activate(true);
    entities.push_back(fireball);

    float now = RenderingMaster::getInstance()->getTime();
    m_fireballs.push_back(std::make_pair(fireball, now));
    /* burn out after a while; keep only the most recent ones */
    while (m_fireballs.size() > maxFireballs) {
        m_fireballs.front().first->setToBeRemoved(true);
        m_fireballs.erase(m_fireballs.begin());
    }
}

void EngineCore::applyMouseLook() {
    /* applied every rendered frame so looking around is not limited to the tick rate */
    glm::vec2 delta = inputManager.consumeMouseDelta();
    if (inputManager.getWarpMouse()) {
        RenderingMaster::getInstance()->rotateCameraX(delta.y * this->mouseRotationSpeed);
        RenderingMaster::getInstance()->rotateCameraY(delta.x * this->mouseRotationSpeed);
    }
}

void EngineCore::capturePreviousState() {
    /* state at the start of the tick = "previous" state for interpolation */
    m_previousTick.entities.clear();
    m_previousTick.lights.clear();
    for (Entity *entity : entities) {
        /* only things that can move between ticks */
        if (entity->getComponent(Entity::Flags::DYNAMIC) != NULL) {
            m_previousTick.entities.emplace(entity, entity->getTransform());
        }
    }
    for (Light *light : RenderingMaster::getInstance()->getLights()) {
        if (light->isAnimated()) {
            m_previousTick.lights.emplace(light, light->getTransform());
        }
    }
    m_previousTick.cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
    m_previousTick.cameraRoll = RenderingMaster::getInstance()->getCamera()->getZRot();
    m_previousTick.valid = true;
}

void EngineCore::beginInterpolation(float alpha) {
    m_stashedEntities.clear();
    m_stashedLights.clear();
    Camera *camera = RenderingMaster::getInstance()->getCamera();
    m_tickCameraPosition = camera->getPosition();
    m_tickCameraRoll = camera->getZRot();
    if (!m_previousTick.valid) {
        return;
    }

    for (Entity *entity : entities) {
        auto previous = m_previousTick.entities.find(entity);
        if (previous != m_previousTick.entities.end()) {
            m_stashedEntities.emplace_back(entity, entity->getTransform());
            entity->getTransform().interpolateWith(previous->second, alpha);
        }
    }
    for (Light *light : RenderingMaster::getInstance()->getLights()) {
        auto previous = m_previousTick.lights.find(light);
        if (previous != m_previousTick.lights.end()) {
            Transform tick = light->getTransform();
            light->getTransform().interpolateWith(previous->second, alpha);
            m_stashedLights.emplace_back(light, tick, light->getTransform());
        }
    }
    camera->setPosition(glm::mix(m_previousTick.cameraPosition, m_tickCameraPosition, alpha));
    /* the walking sway's roll too: set once a tick, it stepped on the frames with two ticks */
    camera->setZRot(glm::mix(m_previousTick.cameraRoll, m_tickCameraRoll, alpha));
    RenderingMaster::getInstance()->waking().applyCamera(camera, RenderingMaster::getInstance()->getTime());
    m_drawnCameraPosition = camera->getPosition();

    /* billboards face the camera as it is in this frame */
    for (Entity *entity : entities) {
        if (Component *billboard = entity->getComponent(Entity::Flags::BILL)) {
            billboard->update();
        }
    }
}

void EngineCore::endInterpolation() {
    /* the simulation continues from the real tick state */
    for (auto &stashed : m_stashedEntities) {
        stashed.first->setTransform(stashed.second);
    }
    for (auto &stashed : m_stashedLights) {
        Light *light = std::get<0>(stashed);
        const Transform &frame = std::get<2>(stashed);
        const Transform &now = light->getTransform();
        /* changed during the frame (its editor panel: its radius, its place): the change stays.
         * (Put back, a flickering light's radius could not be edited: undone every frame) */
        bool edited = now.getPosition() != frame.getPosition() || now.getRotation() != frame.getRotation() ||
                      now.getScale() != frame.getScale();
        if (!edited) {
            light->getTransform() = std::get<1>(stashed);
        }
    }
    m_stashedLights.clear();
    /* the lights taken out during this frame (a fire gone out) can go now */
    RenderingMaster::getInstance()->deleteRemovedLights();
    RenderingMaster::getInstance()->getCamera()->setPosition(m_tickCameraPosition);
    RenderingMaster::getInstance()->getCamera()->setZRot(m_tickCameraRoll);
}

static const char *PACING_MODES[] = {
    "the config: cap on the engine's clock, vsync",
    "cap on the engine's clock, no vsync",
    "vsync, no cap",
    "cap by the driver: every Nth refresh",
    "cap on the engine's clock, vsync, each frame waited for",
    "no vsync, no cap (as fast as it can)",
};

void EngineCore::applyPacingMode() {
    const int count = sizeof(PACING_MODES) / sizeof(PACING_MODES[0]);
    m_pacingMode %= count;
    Display *display = RenderingMaster::getInstance()->getDisplay();
    int refresh = display->getRefreshRate();
    double cap = m_configuredFps;
    int interval = m_configuredVSync ? 1 : 0;
    m_maxRenderFps = m_configuredFps;
    m_waitEachFrame = false;
    switch (m_pacingMode) {
        case 1: interval = 0; break;
        case 2: interval = 1; m_maxRenderFps = 0; break;
        case 3: interval = cap > 0 ? std::max(1, (int) std::lround(refresh / cap)) : 1; m_maxRenderFps = 0; break;
        case 4: interval = 1; m_waitEachFrame = true; break;
        case 5: interval = 0; m_maxRenderFps = 0; break;
        default: break;
    }
    bool ok = display->setSwapInterval(interval);
    printf("Frame pacing %d: %s (swap interval %d%s, cap %.2f)\n", m_pacingMode, PACING_MODES[m_pacingMode],
           interval, ok ? "" : " REFUSED", m_maxRenderFps);
    fflush(stdout);
    m_pacingLabelUntil = RenderingMaster::getInstance()->getTime() + 3.0;
}

void EngineCore::stop() {
    isRunning = false;
}

void EngineCore::input() {
    if (inputManager.getKeyDown(SDLK_ESCAPE)) {
        std::cout << dumpJson() << std::endl;;
        stop ();
    }

    if (inputManager.getKeyDown(SDLK_F1)) {
        setPlayMode(!m_playMode);                   /* F1: open / close the editor GUI */
    }

    if (inputManager.getKeyDown(SDLK_F11)) {
        m_screenshotRequested = true;               /* taken from the next rendered frame */
    }

    if (inputManager.getKeyDown(SDLK_F12)) {
        if (m_recorder.isRecording()) {
            m_recorder.stop(AudioMaster::getInstance()->stopCapture(), AudioMaster::sampleRate());
        } else {
            Display *display = RenderingMaster::getInstance()->getDisplay();
            if (m_recorder.start(display->getWidth(), display->getHeight())) {
                AudioMaster::getInstance()->startCapture();
            }
        }
    }

    if (inputManager.getKeyDown(SDLK_F5)) {
        saveMap(m_savePath);
    }
    if (inputManager.getKeyDown(SDLK_F7)) {
        m_pacingMode++;                             /* F7: the next way of pacing frames (by eye) */
        m_pacingModeChanged = true;
    }
    if (inputManager.getKeyDown(SDLK_F9)) {
        reloadMap(m_savePath);
    }

    /* typing into an editor field (a value, a name): the keys are the field's, not the game's (they
     * switched the debug view, threw fireballs, walked) */
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput) {
        return;
    }

    if (inputManager.getKeyDown(SDLK_g)) {
        throwFireball();
    }

    if (inputManager.getKeyDown(SDLK_t)) {
        RenderingMaster::getInstance()->cycleTimeScale();
    }

    if (inputManager.getKeyDown(SDLK_m)) {
        bool warpMouse = !inputManager.getWarpMouse();
        inputManager.setWarpMouse(warpMouse);
    }

    if (inputManager.getWarpMouse()) {
        if (inputManager.getMouse(1)) {
            spawnBox(RenderingMaster::getInstance()->getCamera()->getPosition());
        }
    }

    if (inputManager.getKeyDown(SDLK_0)) {
        outputType = 10;
    }
    if (inputManager.getKeyDown(SDLK_1)) {
        outputType = 1;
    }
    if (inputManager.getKeyDown(SDLK_2)) {
        outputType = 2;
    }
    if (inputManager.getKeyDown (SDLK_5)) {
        outputType = 5;
    /* GE_OUTPUT_TYPE: start in a debug view (same as keys 1/2/4/5/6/8/0) */
    if (const char *ot = std::getenv("GE_OUTPUT_TYPE")) {
        outputType = std::atoi(ot);
    }
    }
    if (inputManager.getKeyDown(SDLK_8)) {
        outputType = 8;
    }
    if (inputManager.getKeyDown (SDLK_6)) {
        outputType = 6;
    }

    if (inputManager.getKeyDown (SDLK_q)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        glm::vec3 cameraRotation = RenderingMaster::getInstance()->getCamera()->getRotation();
        RenderingMaster::getInstance()->addLightToScene(new SpotLight(Transform(cameraPosition,
                                                                  glm::degrees(cameraRotation),
                                                                  glm::vec3(300.0f, 300.0f, 300.0f)),
                                                        glm::vec3(0.98f, 0.85f, 0.85f), true, true));
        Transform transform2(cameraPosition,
                                glm::vec3(0),
                                glm::vec3(1.0f));
        Entity *newEntity2 = new Entity();
        newEntity2->setTransform(transform2);
        entities.push_back(newEntity2->addComponent(new RenderComponent(RenderingObject::loadObject("__circle__")))
                                                    ->addComponent(new BillboardComponent())
                                                    ->addComponent(new GrabComponent(10.0f)));
    }

    if (inputManager.getKeyDown (SDLK_z)) {
        glm::vec3 cameraPosition = RenderingMaster::getInstance()->getCamera()->getPosition();
        RenderingMaster::getInstance()->addLightToScene(new PointLight(Transform(cameraPosition,
                                                                                  glm::vec3(0),
                                                                                  glm::vec3(500.0f)),
                                                                       glm::vec3(1.0f, 0.7f, 0.2f),
                                                                       true /* casts shadow */));

    }

    if (inputManager.getKeyDown(SDLK_r)) {
        RenderingMaster::getInstance()->volumetricLightShader.reload();
        SpotLight::getLightAccumulationShader().reload();
        PointLight::getLightAccumulationShader().reload();
        DirectionalLight::getLightAccumulationShader().reload();

        //RenderingMaster::getInstance()->resetLights();
        if (RenderingMaster::getInstance()->smokeRenderer != NULL) {
            RenderingMaster::getInstance()->smokeRenderer->getRenderingShader().reload();

            delete RenderingMaster::getInstance()->smokeRenderer;

            RenderingMaster::getInstance()->smokeRenderer = ParticleFactory::createParticleRenderer<SmokeParticle> ("res/particleVolumes/smokeCone.json");
        }
    }

    // ray intersection
    if (inputManager.getKeyUp(SDLK_e)) {
        PhysicsMaster::getInstance()->performRayTestWithCamForward(RenderingMaster::getInstance()->getCamera()->getPosition(),
                                    RenderingMaster::getInstance()->getCamera()->getForward());
    }

    if (!inputManager.getWarpMouse()) {
        PhysicsMaster::getInstance()->performMouseRayIntersection(inputManager.getMousePos().x,
                                                                inputManager.getMousePos().y,
                                                                RenderingMaster::getInstance()->getDisplay()->getWidth(),
                                                                RenderingMaster::getInstance()->getDisplay()->getHeight(),
                                                                RenderingMaster::getInstance()->getCamera()->getPosition(),
                                                                RenderingMaster::getInstance()->getProjectionMatrix(),
                                                                RenderingMaster::getInstance()->getCamera()->getViewMatrix());
    }
    // end ray intersection

    RenderingMaster::getInstance()->deferredShading_BufferCombinationShader.updateUniform("outputType", (void *) &outputType);

    for (auto const &entity : entities) {
        entity->input(inputManager);
    }

    for (auto const &light : RenderingMaster::getInstance()->getLights()) {
        light->input(inputManager);
    }
}

static inline uint64_t fnv1a(uint64_t h, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *) data;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

void EngineCore::render(float alpha) {
    beginInterpolation(alpha);
    /* the map's sequences, then what they drive, then their sounds */
    m_timeline.advance(RenderingMaster::getInstance()->getTime());
    if (m_aftermath.update(RenderingMaster::getInstance()->getCamera(), m_timeline) || m_timeline.fired("quit")) {
        /* the Kugelblitz's ending: the game closes (not under tools/timeline_editor.py: there it is
         * a moment to look at, and gone back from) */
        if (!m_remote.connected()) isRunning = false;
    }
    m_timeline.playSounds(RenderingMaster::getInstance()->getCamera()->getPosition());
    RenderingMaster::getInstance()->prepareFrame(alpha);
    m_soundScape.update(RenderingMaster::getInstance()->getCamera());

    /*generate deferred shading buffers*/
    PT_FromHere("renderScene");
    RenderingMaster::getInstance()->getGBuffer().bindForScene();
    RenderingMaster::getInstance()->clearScreen(1, 1, 1, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* what the camera can see: the rest is not drawn */
    const bool culling = RenderingMaster::viewCulling();
    const ViewPlanes view(RenderingMaster::getInstance()->getProjectionMatrix() *
                          RenderingMaster::getInstance()->getCamera()->getViewMatrix());

    for (auto entity : entities) {
        RenderComponent *renderComponent = NULL;
        InstanceRenderComponent *instanceRenderComponent = NULL;
        SkeletalAnimationComponent *skeletalAnimationComponent = NULL;

        // hack, needs improvements
        if (entity->getComponent(Entity::Flags::BILL) != NULL)
            continue;
        if ((renderComponent =
             (RenderComponent *) (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL &&
            !renderComponent->isBeyondDrawDistance(entity->getTransform()) &&
            !(culling && renderComponent->isOutside(view, entity->getTransform()))) {
            if (renderComponent->isGlass()) {
                RenderingMaster::getInstance()->glass.push_back(entity);   /* drawn after the scene */
            } else {
                renderComponent->render(&RenderingMaster::getInstance()->deferredShading_SceneShader);
            }
        }
        if ((instanceRenderComponent =
             (InstanceRenderComponent *) (entity->getComponent (Entity::Flags::INSTANCE_RENDERABLE))) != NULL) {
            instanceRenderComponent->render(&RenderingMaster::getInstance()->deferredShading_InstanceRender, culling ? &view : nullptr);
        }
        if ((skeletalAnimationComponent =
             (SkeletalAnimationComponent *) (entity->getComponent (Entity::Flags::ANIMATED))) != NULL) {
            skeletalAnimationComponent->render(&RenderingMaster::getInstance()->deferredShading_AnimRender);
        }
    }
    if (RenderingMaster::getInstance()->grass != NULL) {
        PT_FromHere("grass");
        RenderingMaster::getInstance()->grass->render();
        PT_ToHere("grass");
    }
    /* the sky last: only where nothing above covers it */
    PT_FromHere("sky");
    RenderingMaster::getInstance()->drawMainSky();
    PT_ToHere("sky");
    DebugRenderer::getInstance()->render();
    RenderingMaster::getInstance()->getGBuffer().unbind();
    PT_ToHere("renderScene");
    /*end generate deferred shading buffers*/


    if (RenderingMaster::getInstance()->water != NULL) {
        PT_FromHere("waterReflection");
        RenderingMaster::getInstance()->water->renderReflection(entities);
        PT_ToHere("waterReflection");
    }

    const std::vector <Light *> &lights = RenderingMaster::getInstance ()->getLights ();
    PT_FromHere("computeDepthTextureForLight");

    /* Signature of everything drawn into shadow maps. Shadow maps are only
     * re-rendered when this or the light's own matrices change, so static
     * lights over a resting scene cost nothing after the first frame. */
    uint64_t casterSignature = 14695981039346656037ULL;
    for (auto entity : entities) {
        if (entity->getComponent(Entity::Flags::BILL) != NULL) {
            continue;
        }
        RenderComponent *casterRender = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
        if ((casterRender == NULL || !casterRender->castsShadow()) &&
            entity->getComponent(Entity::Flags::INSTANCE_RENDERABLE) == NULL) {
            continue; /* not drawn into shadow maps: moving it must not invalidate them */
        }
        const glm::mat4 &model = entity->getTransform().getModelMatrix();
        casterSignature = fnv1a(casterSignature, &entity, sizeof(entity));
        casterSignature = fnv1a(casterSignature, &model, sizeof(model));
    }

    /* begin drawing the spot light depth map for each light*/
    for (Light *light : lights) {
        if (light->isCastingShadow()) {
            PointLight *pointLight = dynamic_cast<PointLight *>(light);
            uint64_t signature = casterSignature;
            if (pointLight != NULL) {
                glm::vec4 positionAndRange(light->getTransform().getPosition(), pointLight->getRange());
                signature = fnv1a(signature, &positionAndRange, sizeof(positionAndRange));
            } else {
                signature = fnv1a(signature, &light->getShadowMapViewMatrix(), sizeof(glm::mat4));
                signature = fnv1a(signature, &light->getShadowMapProjectionMatrix(), sizeof(glm::mat4));
                /* swaying trees: their shadows move every frame */
                if (InstanceRenderComponent::s_windyComponents > 0 &&
                    RenderingMaster::getInstance()->getEnvironment().wind > 0.0f &&
                    dynamic_cast<DirectionalLight *>(light) != NULL) {
                    float now = RenderingMaster::getInstance()->getTime();
                    signature = fnv1a(signature, &now, sizeof(now));
                }
            }
            if (light->m_shadowSignatureValid && light->m_shadowSignature == signature) {
                continue;
            }
            light->m_shadowSignature = signature;
            light->m_shadowSignatureValid = true;

            if (pointLight != NULL) {
                pointLight->renderShadowCube(entities);
                continue;
            }

            RenderingMaster::getInstance()->beginCreateDepthTextureForLight(light);
            for (auto entity : entities) {
                RenderComponent *renderComponent = NULL;
                InstanceRenderComponent *instanceRenderComponent = NULL;

                // hack, needs improvements
                if (entity->getComponent(Entity::Flags::BILL) != NULL)
                    continue;
                if ((renderComponent =
                     (RenderComponent *) (entity->getComponent (Entity::Flags::RENDERABLE))) != NULL &&
                    renderComponent->castsShadow()) {
                    renderComponent->render(&RenderingMaster::getInstance()->depthMapCreator);
                }
                if ((instanceRenderComponent =
                    (InstanceRenderComponent *) (entity->getComponent (Entity::Flags::INSTANCE_RENDERABLE))) != NULL) {
                    instanceRenderComponent->render(&RenderingMaster::getInstance()->depthMapCreatorInstanceRender);
                }
            }
            RenderingMaster::getInstance()->endCreateDepthTextureForLight(light);
        }
    }
    /* end drawing the light depth map */
    PT_ToHere("computeDepthTextureForLight");

    PT_FromHere("lightAcc");
    /* the crowns the sun casts soft shadows of where its shadow map does not reach: the nearest
     * ones when there are more than the light pass takes */
    {
        std::vector<glm::vec4> crowns;
        for (auto entity : entities) {
            InstanceRenderComponent *instances = (InstanceRenderComponent *) entity->getComponent(Entity::Flags::INSTANCE_RENDERABLE);
            if (instances != NULL) {
                instances->appendCrownShadows(crowns);
            }
        }
        if (crowns.size() / 2 > (std::size_t) DirectionalLight::MAX_CROWNS) {
            glm::vec3 eye = RenderingMaster::getInstance()->getCamera()->getPosition();
            std::vector<std::size_t> order(crowns.size() / 2);
            for (std::size_t i = 0; i < order.size(); i++) order[i] = i;
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                return glm::distance(glm::vec3(crowns[a * 2]), eye) < glm::distance(glm::vec3(crowns[b * 2]), eye);
            });
            std::vector<glm::vec4> nearest;
            for (int i = 0; i < DirectionalLight::MAX_CROWNS; i++) {
                nearest.push_back(crowns[order[i] * 2]);
                nearest.push_back(crowns[order[i] * 2 + 1]);
            }
            crowns.swap(nearest);
        }
        DirectionalLight::setCrownShadows(crowns);
    }
    RenderingMaster::getInstance()->createLightAccumulationBuffer();
    PT_ToHere("lightAcc");

    RenderingMaster::getInstance()->getGBuffer().unbind();

    PT_FromHere("particleDraw");
    RenderingMaster::getInstance()->particleForwardRenderFramebuffer.bindAllRenderTargets();
    RenderingMaster::getInstance()->depthTexture->use();
    RenderingMaster::getInstance()->drawParticles();
    RenderingMaster::getInstance()->particleForwardRenderFramebuffer.unbind();
    PT_ToHere("particleDraw");

    PT_FromHere("FlaresDraw");
    {
        int flareSources = 0;
        for (auto entity : entities) {
            if (entity->getComponent(Entity::Flags::BILL) != NULL) {
                flareSources++;
            }
        }
        if (RenderingMaster::getInstance()->isSunFlareVisible()) {
            flareSources++;
        }
        RenderingMaster::getInstance()->setFlareSourceCount(flareSources);
    }
    if (RenderingMaster::getInstance()->flaresNeeded()) {
        RenderingMaster::getInstance()->spotLightFlares.bindAllRenderTargets();
        RenderingMaster::getInstance()->depthTexture->use(0);
        for (auto entity : entities) {
            if (entity->getComponent(Entity::Flags::BILL) != NULL) {
                RenderComponent *renderComponent = (RenderComponent *) (entity->getComponent(Entity::Flags::RENDERABLE));
                renderComponent->render(&RenderingMaster::getInstance()->flareShader);
            }
        }
        RenderingMaster::getInstance()->drawSunFlareSource();
        RenderingMaster::getInstance()->spotLightFlares.unbind();
    }
    PT_ToHere("FlaresDraw");

    PT_FromHere_GPU("volumetric");
    RenderingMaster::getInstance()->renderVolumetricLight();
    PT_ToHere_GPU("volumetric");

    PT_FromHere("buffersDraw");
    RenderingMaster::getInstance()->drawDeferredShadingBuffers();
    PT_ToHere ("buffersDraw");
    /* the finished image, before any overlay. GE_RECORD=1 records from the
     * first frame (scripted captures) */
    static bool autoRecord = std::getenv("GE_RECORD") != NULL;
    if (autoRecord) {
        autoRecord = false;
        Display *display = RenderingMaster::getInstance()->getDisplay();
        if (m_recorder.start(display->getWidth(), display->getHeight())) {
            AudioMaster::getInstance()->startCapture();
        }
    }
    m_recorder.captureFrame();
    if (m_screenshotRequested) {
        m_screenshotRequested = false;
        takeScreenshot();
    }

    /* GE_CLEAN_SHOTS=1: no profiler bars, FPS text or ImGui windows (clean
     * screenshots, e.g. tools/visual_tour.py); F1 does the same while playing */
    static const bool cleanShots = std::getenv("GE_CLEAN_SHOTS") != NULL;
    if (!cleanShots && !m_playMode) {
    ProfilingTimer::renderAllBarGUIs();
    fpsGUI->render();
    RenderingMaster::getInstance()->startIMGUIFrame();
    for (auto entity : entities) {
        GrabComponent *grabComponent;
        if ((grabComponent =
             (GrabComponent *) (entity->getComponent (Entity::Flags::GRAB))) != NULL) {
            grabComponent->render();
        }
    }
    for (auto const &light : RenderingMaster::getInstance()->getLights()) {
        light->renderGUI();
    }
    RenderingMaster::getInstance()->imguiDrawCalls();
    if (m_recorder.isRecording()) {
        /* not in the video: it is captured before the overlays */
        int seconds = (int) m_recorder.elapsedSeconds();
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::Begin("Recording", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                           ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "REC %02d:%02d  (F12 to stop)", seconds / 60, seconds % 60);
        ImGui::End();
    }
    RenderingMaster::getInstance()->renderIMGUI();
    }
    if (!cleanShots && m_playMode && RenderingMaster::getInstance()->getTime() < m_pacingLabelUntil) {
        /* F7 while playing: which way frames are paced now */
        RenderingMaster::getInstance()->startIMGUIFrame();
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 8.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::Begin("Frame pacing", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                              ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Frame pacing %d: %s", m_pacingMode, PACING_MODES[m_pacingMode]);
        ImGui::End();
        RenderingMaster::getInstance()->renderIMGUI();
    }

    if (m_captureBeforeSwap != NULL) {
        Display *display = RenderingMaster::getInstance()->getDisplay();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, display->getWidth(), display->getHeight(),
                     GL_RGBA, GL_UNSIGNED_BYTE, m_captureBeforeSwap->data());
    }

    PT_FromHere("swapBuffers");
    RenderingMaster::getInstance()->swapBuffers();
    PT_ToHere("swapBuffers");

    ProfilingTimer::updateGPUTimers();
    endInterpolation();
}

void EngineCore::update() {
    for (auto it = m_fireballs.begin(); it != m_fireballs.end();) {
        if (RenderingMaster::getInstance()->getTime() - it->second > 20.0f) {
            it->first->setToBeRemoved(true);
            it = m_fireballs.erase(it);
        } else {
            ++it;
        }
    }

    if (RenderingMaster::getInstance()->isWorldPaused()) {
        /* the timeline editor's pause: nothing moves; the storm's settings still apply */
        RenderingMaster::getInstance()->update();
        return;
    }

    std::vector<Light *> &lights = RenderingMaster::getInstance()->getLights();

    auto entity = std::begin(entities);
    while (entity != std::end(entities)) {
        (*entity)->update();
        if ((*entity)->isToBeRemoved() == true) {
            /* delete it too: its components (physics body, emitters) must go away */
            m_previousTick.entities.erase(*entity);
            delete *entity;
            entity = entities.erase(entity);
        } else {
            ++entity;
        }
    }

    auto light = std::begin(lights);
    while (light != std::end(lights)) {
        (*light)->update();
        if ((*light)->isToBeRemoved() == true) {
            light = lights.erase(light);
        } else {
            ++light;
        }
    }

    PhysicsMaster::getInstance()->update();
    RenderingMaster::getInstance()->update();

    RenderingMaster::getInstance()->secondFraction = secondFraction;
    // findPathToPlayer();
}

std::vector<Entity *> &EngineCore::getEntities() {
    return entities;
}

void EngineCore::constructPlayer() {
    Transform playerTrans (m_playerStart, glm::vec3(0), glm::vec3(10));
    RenderingMaster::getInstance()->getCamera()->setRotation(glm::radians(m_playerPitch), glm::radians(m_playerYaw), 0.0f);
    PhysicsComponent *playerPhysicsComponent = new PhysicsComponent (PhysicsComponent::BoundingBodyType::CAPSULE,
                                                                    glm::vec3(5.0, 10.0f, 10.0f),
                                                                    30.0f);
    player = (Player *) (new Player(playerTrans))
                        ->addComponent(playerPhysicsComponent);
    entities.push_back(player);
    playerPhysicsComponent->getRigidBody()->setDamping(btScalar(0.5), btScalar(0.0));
    playerPhysicsComponent->getRigidBody()->setSleepingThresholds(0.0, 0.0);
    playerPhysicsComponent->getRigidBody()->setAngularFactor(0.0);
    playerPhysicsComponent->getRigidBody()->setFriction(0.5);
    playerPhysicsComponent->getRigidBody()->setRestitution(0.6);
}

void EngineCore::constructEnemy() {
    Transform enemyTrans(glm::vec3(300, 2.5, 100), glm::vec3(0), glm::vec3(0.3f));

    enemy = new Entity();
    enemy->setTransform(enemyTrans);

    RenderingObject obj = RenderingObject::loadObject("res/models/BaseMesh_Anim.dae", true, true);

    PhysicsComponent *enemyPhysicsComponent = new PhysicsComponent (PhysicsComponent::BoundingBodyType::CAPSULE,
                                                                    glm::vec3(7.0, 20.0f, 7.0f),
                                                                    30.0f,
                                                                    false);
    enemy = enemy->addComponent(new SkeletalAnimationComponent(std::move(obj)))
                    ->addComponent(new AIPlayerFollowerComponent(player))
                    ->addComponent(enemyPhysicsComponent);

    enemyPhysicsComponent->getRigidBody()->setDamping(btScalar(0.5), btScalar(0.0));
    enemyPhysicsComponent->getRigidBody()->setSleepingThresholds(0.0, 0.0);
    enemyPhysicsComponent->getRigidBody()->setAngularFactor(0.0);
    enemyPhysicsComponent->getRigidBody()->setFriction(0.5);
    enemyPhysicsComponent->getRigidBody()->setRestitution(0.6);

    entities.push_back(enemy);
}

void EngineCore::loadLights(rapidjson::Document &gameDocument) {
    if (gameDocument.HasMember("Lights")) {
        unsigned int numberOfLights = gameDocument["Lights"].GetArray().Size();
        unsigned int step = 0;
        for (auto const &light : gameDocument["Lights"].GetArray()) {
            step++;
            std::cout << "Loading lights...%" << (float) step * 100 / numberOfLights << std::endl;
            glm::vec3 position(0), rotation(0), scale(1);
            glm::quat rotation_quat(0,0,0,1);
            Transform trans;

            if (light.HasMember("Transform")) {
                position.x = light["Transform"]["position"].GetArray()[0].GetFloat();
                position.y = light["Transform"]["position"].GetArray()[1].GetFloat();
                position.z = light["Transform"]["position"].GetArray()[2].GetFloat();

                trans.setPosition(position);

                if (light["Transform"]["rotation"].GetArray().Size() == 3) {
                    rotation.x = light["Transform"]["rotation"].GetArray()[0].GetFloat();
                    rotation.y = light["Transform"]["rotation"].GetArray()[1].GetFloat();
                    rotation.z = light["Transform"]["rotation"].GetArray()[2].GetFloat();

                    trans.setRotation(rotation);
                } else if (light["Transform"]["rotation"].GetArray().Size() == 4) {
                    rotation_quat.x = light["Transform"]["rotation"].GetArray()[0].GetFloat();
                    rotation_quat.y = light["Transform"]["rotation"].GetArray()[1].GetFloat();
                    rotation_quat.z = light["Transform"]["rotation"].GetArray()[2].GetFloat();
                    rotation_quat.w = light["Transform"]["rotation"].GetArray()[3].GetFloat();

                    trans.setRotation(rotation_quat);
                }

                if (light["Transform"]["scale"].GetArray().Size() == 1) {
                    scale.x = light["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.y = light["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.z = light["Transform"]["scale"].GetArray()[0].GetFloat();
                } else {
                    scale.x = light["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.y = light["Transform"]["scale"].GetArray()[1].GetFloat();
                    scale.z = light["Transform"]["scale"].GetArray()[2].GetFloat();
                }

                trans.setScale(scale);
            }
            Light *newLight = NULL;
            glm::vec3 color(0,0,0);
            glm::vec3 lightDir(0,0,0);
            std::string type = "N/A"; //SPOT, POINT or DIR
            bool cast_shadow = false, volumetric = false;

            if (light.HasMember("Attributes")) {
                const auto &attribs = light["Attributes"].GetObject();

                if (attribs.HasMember("type")) {
                    type = std::string(attribs["type"].GetString());
                }
                if (attribs.HasMember("color")) {
                    color.x = attribs["color"].GetArray()[0].GetFloat();
                    color.y = attribs["color"].GetArray()[1].GetFloat();
                    color.z = attribs["color"].GetArray()[2].GetFloat();
                }
                if (attribs.HasMember("dir")) {
                    lightDir.x = attribs["dir"].GetArray()[0].GetFloat();
                    lightDir.y = attribs["dir"].GetArray()[1].GetFloat();
                    lightDir.z = attribs["dir"].GetArray()[2].GetFloat();
                }
                if (attribs.HasMember("cast_shadow")) {
                    cast_shadow = attribs["cast_shadow"].GetBool();
                }
                if (attribs.HasMember("volumetric")) {
                    volumetric = attribs["volumetric"].GetBool();
                }
            }

            if (type.compare("SPOT") == 0) {
                int shadowMapSize = 2048;
                if (light.HasMember("Attributes") && light["Attributes"].HasMember("shadowMapSize")) {
                    shadowMapSize = light["Attributes"]["shadowMapSize"].GetInt();
                }
                newLight = new SpotLight(trans, color, cast_shadow, volumetric, shadowMapSize);
            } else if (type.compare("POINT") == 0) {
                int shadowMapSize = 512;
                if (light.HasMember("Attributes") && light["Attributes"].HasMember("shadowMapSize")) {
                    shadowMapSize = light["Attributes"]["shadowMapSize"].GetInt();
                }
                newLight = new PointLight(trans, color, cast_shadow, shadowMapSize);
            } else if (type.compare("DIR") == 0) {
                newLight = new DirectionalLight(trans, color, lightDir, cast_shadow);
            } else {
                continue;
            }

            if (light.HasMember("Attributes")) {
                const auto &attribs = light["Attributes"].GetObject();

                if (attribs.HasMember("diffuse_attenuation")) {
                    newLight->setAttDiffuse(attribs["diffuse_attenuation"].GetArray()[0].GetFloat(),
                                            attribs["diffuse_attenuation"].GetArray()[1].GetFloat(),
                                            attribs["diffuse_attenuation"].GetArray()[2].GetFloat());
                }

                if (attribs.HasMember("specular_attenuation")) {
                    newLight->setAttSpecular(attribs["specular_attenuation"].GetArray()[0].GetFloat(),
                                            attribs["specular_attenuation"].GetArray()[1].GetFloat(),
                                            attribs["specular_attenuation"].GetArray()[2].GetFloat());
                }

                Light::Animation animation;
                if (attribs.HasMember("flicker")) {
                    animation.flicker = attribs["flicker"].GetFloat();
                }
                if (attribs.HasMember("flickerSpeed")) {
                    animation.flickerSpeed = attribs["flickerSpeed"].GetFloat();
                }
                if (attribs.HasMember("spin")) {
                    animation.spin = attribs["spin"].GetFloat();
                }
                if (attribs.HasMember("nightOnly")) {
                    animation.nightOnly = attribs["nightOnly"].GetBool();
                }
                if (attribs.HasMember("orbit") && attribs["orbit"].IsObject()) {
                    const auto &orbit = attribs["orbit"];
                    animation.orbit = true;
                    animation.orbitCenter = trans.getPosition();
                    if (orbit.HasMember("center")) {
                        animation.orbitCenter = glm::vec3(orbit["center"][0].GetFloat(),
                                                          orbit["center"][1].GetFloat(),
                                                          orbit["center"][2].GetFloat());
                    }
                    animation.orbitRadius = orbit.HasMember("radius") ? orbit["radius"].GetFloat() : 50.0f;
                    animation.orbitSpeed = orbit.HasMember("speed") ? orbit["speed"].GetFloat() : 20.0f;
                    animation.orbitBob = orbit.HasMember("bob") ? orbit["bob"].GetFloat() : 0.0f;
                    animation.orbitPhase = orbit.HasMember("phase") ? orbit["phase"].GetFloat() : 0.0f;
                }
                newLight->setAnimation(animation);

                if (type.compare("SPOT") == 0) {
                    SpotLight *spot = dynamic_cast<SpotLight *>(newLight);

                    if (spot != NULL) {
                        if (attribs.HasMember("numSamplePoints")) {
                            spot->setNumSamplePoints(attribs["numSamplePoints"].GetInt());
                        }
                        if (attribs.HasMember("coef1")) {
                            spot->setCoef1(attribs["coef1"].GetFloat());
                        }
                        if (attribs.HasMember("coef2")) {
                            spot->setCoef2(attribs["coef2"].GetFloat());
                        }
                        if (attribs.HasMember("beamDensity")) {
                            spot->setBeamDensity(attribs["beamDensity"].GetFloat());
                        }
                        if (attribs.HasMember("beamFalloff")) {
                            spot->setBeamFalloff(attribs["beamFalloff"].GetFloat());
                        }
                        if (attribs.HasMember("beamHaze")) {
                            spot->setBeamHaze(attribs["beamHaze"].GetFloat());
                        }
                        if (attribs.HasMember("coneAngle")) {
                            spot->setConeAngle(attribs["coneAngle"].GetFloat());
                        }
                    }
                }
            }

            RenderingMaster::getInstance()->addLightToScene(newLight);
        }
    }
}

void EngineCore::loadGrass(rapidjson::Document &gameDocument) {
    if (!gameDocument.HasMember("Grass") || !gameDocument["Grass"].IsObject()) {
        return;
    }
    /* grows on the terrain: the entity whose material is a terrain material */
    for (Entity *entity : entities) {
        RenderComponent *rc = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
        if (rc == NULL || rc->getRenderingObject().getMaterials().empty()) {
            continue;
        }
        const TerrainLayers *terrain = rc->getRenderingObject().getMaterials()[0]->getTerrain();
        if (terrain == nullptr) {
            continue;
        }
        GrassRenderer *grass = new GrassRenderer(gameDocument["Grass"], rc->getRenderingObject().getFilePath(),
                                                 entity->getTransform().getModelMatrix(), terrain);
        if (grass->isValid()) {
            RenderingMaster::getInstance()->grass = grass;
        } else {
            delete grass;
        }
        return;
    }
    std::cout << "Grass: the map has no terrain entity to grow on" << std::endl;
}

void EngineCore::loadMapFeatures(rapidjson::Document &gameDocument) {
    RenderingMaster *rm = RenderingMaster::getInstance();

    if (gameDocument.HasMember("Environment")) {
        Environment environment;
        environment.load(gameDocument["Environment"]);
        /* GE_TIME_OF_DAY=0.5: start at this time of day (testing / screenshots) */
        if (const char *timeOfDay = std::getenv("GE_TIME_OF_DAY")) {
            environment.timeOfDay = (float) std::atof(timeOfDay);
        }
        rm->setEnvironment(environment);
    }

    if (gameDocument.HasMember("Audio")) {
        m_soundScape.load(gameDocument["Audio"]);
    }

    /* the map's footsteps: their sound and the dust they kick up */
    if (gameDocument.HasMember("Footsteps") && gameDocument["Footsteps"].IsObject()) {
        const rapidjson::Value &steps = gameDocument["Footsteps"];
        Player::Footsteps &footsteps = Player::footsteps();
        if (steps.HasMember("sound") && steps["sound"].IsString()) footsteps.sound = steps["sound"].GetString();
        if (steps.HasMember("variants") && steps["variants"].IsInt()) footsteps.variants = std::max(1, steps["variants"].GetInt());
        if (steps.HasMember("volume") && steps["volume"].IsNumber()) footsteps.volume = steps["volume"].GetFloat();
        if (steps.HasMember("dustPerStep") && steps["dustPerStep"].IsInt()) footsteps.dustPerStep = steps["dustPerStep"].GetInt();
        if (steps.HasMember("dust") && steps["dust"].IsObject()) {
            footsteps.dust = new ParticleEmitter(steps["dust"]);
            rm->addParticleEmitter(footsteps.dust);
        }
    }
    /* the step sounds loaded now, not on the first steps (each variant read and converted the first
     * time it played: a hitch on the first few steps) */
    for (int variant = 1; variant <= Player::footsteps().variants; variant++) {
        AudioMaster::getInstance()->load(Player::footsteps().sound + "_" + std::to_string(variant));
    }

    if (gameDocument.HasMember("BlackHoles") && gameDocument["BlackHoles"].IsArray()) {
        for (auto const &hole : gameDocument["BlackHoles"].GetArray()) {
            /* in the sky only: a hole placed on the ground (no "sky": true) is no longer drawn */
            if (!hole.IsObject() || !hole.HasMember("sky") || !hole["sky"].IsBool() || !hole["sky"].GetBool()) {
                std::cout << "BlackHoles: an entry without \"sky\": true skipped (black holes are in the sky only)" << std::endl;
                continue;
            }
            rm->blackHoles.push_back(new BlackHole(hole));
        }
    }

    /* the map's scripted sequences; a Moon destruction without any of its own
     * gets the default storm after it */
    m_timeline.clear();
    if (gameDocument.HasMember("Sequences")) {
        m_timeline.load(gameDocument["Sequences"]);
    } else {
        for (BlackHole *hole : rm->blackHoles) {
            if (hole->sky && hole->kugelblitz) {
                m_timeline.loadFile("Game/sequences/kugelblitz_storm.json");
                break;
            }
        }
    }

    if (gameDocument.HasMember("Water") && gameDocument["Water"].IsObject()) {
        rm->water = new WaterRenderer(gameDocument["Water"], rm->getRenderWidth(), rm->getRenderHeight());
    }

    if (gameDocument.HasMember("ParticleEmitters") && gameDocument["ParticleEmitters"].IsArray()) {
        for (auto const &emitter : gameDocument["ParticleEmitters"].GetArray()) {
            rm->particleEmitters.push_back(new ParticleEmitter(emitter));
        }
    }

    if (gameDocument.HasMember("PlayerStart") && gameDocument["PlayerStart"].IsObject()) {
        const auto &start = gameDocument["PlayerStart"];
        if (start.HasMember("position")) {
            m_playerStart = glm::vec3(start["position"][0].GetFloat(),
                                      start["position"][1].GetFloat(),
                                      start["position"][2].GetFloat());
        }
        m_playerYaw = start.HasMember("yaw") ? start["yaw"].GetFloat() : 0.0f;
        m_playerPitch = start.HasMember("pitch") ? start["pitch"].GetFloat() : 0.0f;
    }
    /* GE_START="x,y,z,yaw,pitch" overrides the spawn (handy for screenshots) */
    if (const char *start = std::getenv("GE_START")) {
        std::sscanf(start, "%f,%f,%f,%f,%f", &m_playerStart.x, &m_playerStart.y, &m_playerStart.z,
                    &m_playerYaw, &m_playerPitch);
    }
    rm->waking().load(gameDocument, m_playerStart, m_playerYaw, m_playerPitch);   /* (a waking-up scene) */
    rm->roomAlive().load(gameDocument);                                         /* (the room coming alive) */
    /* GE_SKIP_ANIM=1: the map's animations skipped: the scene as it is at their end (its timeline's
     * end, past the sky's Moon destruction if that plays on its own), as if played to there */
    if (std::getenv("GE_SKIP_ANIM") != nullptr) {
        float end = m_timeline.sceneLength();
        for (BlackHole *hole : rm->blackHoles) {
            if (hole->sky && hole->kugelblitz && !hole->driven) end = std::max(end, hole->formationTime() + 1.0f);
        }
        seekScene(end);
    }
}

void EngineCore::spawnBox(const glm::vec3 &position) {
    Transform transform(position,
                        glm::vec3(0),
                        glm::vec3(20));
    Entity *newEntity = new Entity();
    newEntity->setTransform(transform);
    entities.push_back(newEntity->addComponent(new RenderComponent(makeSpawnedBox()))
                                ->addComponent(new PhysicsComponent(PhysicsComponent::BoundingBodyType::CUBE,
                                                                    glm::vec3(20),
                                                                    20.0f)));
}

/* A box for the left-click spawner: textured and normal mapped, cycling
 * through a few material sets so stacks of boxes do not all look alike.
 * (cube4.obj has UVs; the loader computes the tangents for normal mapping.) */
RenderingObject EngineCore::makeSpawnedBox() {
    struct BoxLook {
        const char *albedo, *normal;
        float roughness, metallic, specularStrength;
    };
    static const BoxLook looks[] = {
        { "res/textures/brick_wall.jpg", "res/textures/brick_wall_normal.jpg", 0.85f, 0.0f, 0.3f },
        { "res/textures/metal.jpg",      "res/textures/metal_normal.jpg",      0.4f,  0.8f, 1.0f },
        { "res/textures/196.bmp",        "res/textures/196_norm.bmp",          0.75f, 0.0f, 0.5f },
        { "res/textures/165.bmp",        "res/textures/165_norm.bmp",          0.8f,  0.0f, 0.4f },
    };
    static unsigned int next = 0;
    const BoxLook &look = looks[next++ % (sizeof(looks) / sizeof(looks[0]))];

    Material *material = new Material();
    material->setShininess(100.0f);
    material->setRoughness(look.roughness);
    material->setMetallic(look.metallic);
    material->setSpecularStrength(look.specularStrength);
    material->setDiffuseTexture(new Texture(look.albedo, 0));
    material->setNormalTexture(new Texture(look.normal, 1));

    RenderingObject box = RenderingObject::loadObject("res/models/cube4.obj", true, false);
    box.addMaterial(material);
    return box;
}

void EngineCore::loadEntities(rapidjson::Document &gameDocument) {
    if (gameDocument.HasMember("Entities")) {
        unsigned int numberOfEntities = gameDocument["Entities"].GetArray().Size();
        unsigned int step = 0;
        for (auto const &entity : gameDocument["Entities"].GetArray()) {
            step++;
            std::cout << "Loading entities...%" << (float) step * 100 / numberOfEntities << std::endl;
            glm::vec3 position(0), rotation(0), scale(1);
            glm::quat rotation_quat(0,0,0,1);
            Transform trans;

            if (entity.HasMember("Transform")) {
                position.x = entity["Transform"]["position"].GetArray()[0].GetFloat();
                position.y = entity["Transform"]["position"].GetArray()[1].GetFloat();
                position.z = entity["Transform"]["position"].GetArray()[2].GetFloat();

                trans.setPosition(position);

                if (entity["Transform"]["rotation"].GetArray().Size() == 3) {
                    rotation.x = entity["Transform"]["rotation"].GetArray()[0].GetFloat();
                    rotation.y = entity["Transform"]["rotation"].GetArray()[1].GetFloat();
                    rotation.z = entity["Transform"]["rotation"].GetArray()[2].GetFloat();

                    trans.setRotation(rotation);
                } else if (entity["Transform"]["rotation"].GetArray().Size() == 4) {
                    rotation_quat.x = entity["Transform"]["rotation"].GetArray()[0].GetFloat();
                    rotation_quat.y = entity["Transform"]["rotation"].GetArray()[1].GetFloat();
                    rotation_quat.z = entity["Transform"]["rotation"].GetArray()[2].GetFloat();
                    rotation_quat.w = entity["Transform"]["rotation"].GetArray()[3].GetFloat();

                    trans.setRotation(rotation_quat);
                }

                if (entity["Transform"]["scale"].GetArray().Size() == 1) {
                    scale.x = entity["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.y = entity["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.z = entity["Transform"]["scale"].GetArray()[0].GetFloat();
                } else {
                    scale.x = entity["Transform"]["scale"].GetArray()[0].GetFloat();
                    scale.y = entity["Transform"]["scale"].GetArray()[1].GetFloat();
                    scale.z = entity["Transform"]["scale"].GetArray()[2].GetFloat();
                }

                trans.setScale(scale);
            }
            Entity *newEntity = new Entity ();
            newEntity->setTransform (trans);
            newEntity->setSourceJson (jsonToString(entity));
            if (entity.HasMember("Components")) {
                for (rapidjson::Value::ConstMemberIterator itr = entity["Components"].GetObject().MemberBegin();
                        itr != entity["Components"].GetObject().MemberEnd(); ++itr) {
                    Component *component = ComponentFactory::createComponent(itr);
                    newEntity->addComponent(component);
                }
            }
            entities.push_back(newEntity);
        }
    }

    std::cout << "Number of entities: " << entities.size() << std::endl;
}

static void writeTransform(rapidjson::Value &object, const Transform &transform,
                           rapidjson::Document::AllocatorType &allocator) {
    const glm::vec3 &p = transform.getPosition();
    const glm::quat &r = transform.getRotation();
    const glm::vec3 &s = transform.getScale();
    rapidjson::Value t(rapidjson::kObjectType), position(rapidjson::kArrayType), rotation(rapidjson::kArrayType),
                     scale(rapidjson::kArrayType);
    position.PushBack(p.x, allocator).PushBack(p.y, allocator).PushBack(p.z, allocator);
    rotation.PushBack(r.x, allocator).PushBack(r.y, allocator).PushBack(r.z, allocator).PushBack(r.w, allocator);
    scale.PushBack(s.x, allocator).PushBack(s.y, allocator).PushBack(s.z, allocator);
    t.AddMember("position", position, allocator);
    t.AddMember("rotation", rotation, allocator);
    t.AddMember("scale", scale, allocator);
    if (object.HasMember("Transform")) {
        object["Transform"] = t;
    } else {
        object.AddMember("Transform", t, allocator);
    }
}

bool EngineCore::saveMap(const std::string &path) {
    std::string json = mapJson();
    FILE *file = fopen(path.c_str(), "w");
    if (file == NULL) {
        std::cout << "Could not save the map to " << path << std::endl;
        return false;
    }
    fputs(json.c_str(), file);
    fclose(file);
    std::cout << "Map saved to " << path << std::endl;
    return true;
}

/* the whole map as it is now, loadable as a map file (F5 saves it, Escape
 * prints it) */
std::string EngineCore::mapJson() {
    rapidjson::Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();
    RenderingMaster *rm = RenderingMaster::getInstance();

    doc.AddMember("ConfigFilePath", rapidjson::Value(m_configFilePath.c_str(), allocator), allocator);
    doc.AddMember("gravity", m_gravity, allocator);

    /* where you are now becomes the spawn point */
    rapidjson::Value start(rapidjson::kObjectType), position(rapidjson::kArrayType);
    const glm::vec3 &playerPosition = player->getTransform().getPosition();
    const glm::vec3 &cameraRotation = rm->getCamera()->getRotation();
    position.PushBack(playerPosition.x, allocator).PushBack(playerPosition.y + 5.0f, allocator).PushBack(playerPosition.z, allocator);
    start.AddMember("position", position, allocator);
    start.AddMember("yaw", glm::degrees(cameraRotation.y), allocator);
    start.AddMember("pitch", glm::degrees(cameraRotation.x), allocator);
    doc.AddMember("PlayerStart", start, allocator);

    rapidjson::Value environment;
    rm->getEnvironmentToSave().save(environment, allocator);
    doc.AddMember("Environment", environment, allocator);

    auto parseInto = [&](const std::string &json, rapidjson::Value &out) -> bool {
        rapidjson::Document part;
        if (json.empty() || part.Parse(json.c_str()).HasParseError()) {
            return false;
        }
        out.CopyFrom(part, allocator);
        return true;
    };
    rapidjson::Value water, emitters;
    if (parseInto(m_waterJson, water)) doc.AddMember("Water", water, allocator);
    rapidjson::Value waking;
    if (parseInto(m_wakingJson, waking)) doc.AddMember("Waking", waking, allocator);
    rapidjson::Value roomAlive;
    if (parseInto(m_roomAliveJson, roomAlive)) doc.AddMember("RoomAlive", roomAlive, allocator);
    rapidjson::Value grass;
    if (parseInto(m_grassJson, grass)) doc.AddMember("Grass", grass, allocator);
    rapidjson::Value audio;
    if (parseInto(m_audioJson, audio)) doc.AddMember("Audio", audio, allocator);
    rapidjson::Value footsteps;
    if (parseInto(m_footstepsJson, footsteps)) doc.AddMember("Footsteps", footsteps, allocator);
    rapidjson::Value sequences;
    if (parseInto(m_sequencesJson, sequences)) doc.AddMember("Sequences", sequences, allocator);
    if (parseInto(m_emittersJson, emitters)) doc.AddMember("ParticleEmitters", emitters, allocator);

    rapidjson::Value entityArray(rapidjson::kArrayType);
    for (Entity *entity : entities) {
        bool temporary = false;
        for (auto &fireball : m_fireballs) temporary |= fireball.first == entity;
        if (temporary || entity == player) {
            continue;
        }
        rapidjson::Value value;
        if (!entity->getSourceJson().empty()) {
            parseInto(entity->getSourceJson(), value);
        } else {
            /* spawned at runtime: serialize what the engine knows */
            std::string json = entity->jsonify();
            if (json.empty() || !parseInto("{" + json + "}", value)) {
                continue;
            }
        }
        writeTransform(value, entity->getTransform(), allocator);
        entityArray.PushBack(value, allocator);
    }
    doc.AddMember("Entities", entityArray, allocator);

    rapidjson::Value lightArray(rapidjson::kArrayType);
    for (Light *light : rm->getLights()) {
        rapidjson::Value value;
        if (light->isOwnedByFeature() || !parseInto("{" + light->jsonify() + "}", value)) {
            continue;
        }
        glm::vec3 baseColor, baseDirection;
        if (dynamic_cast<DirectionalLight *>(light) != NULL && rm->getDirectionalLightBase(baseColor, baseDirection)) {
            /* the day-night cycle drives it at runtime: save the configured moonlight */
            rapidjson::Value color(rapidjson::kArrayType), dir(rapidjson::kArrayType);
            color.PushBack(baseColor.x, allocator).PushBack(baseColor.y, allocator).PushBack(baseColor.z, allocator);
            dir.PushBack(baseDirection.x, allocator).PushBack(baseDirection.y, allocator).PushBack(baseDirection.z, allocator);
            value["Attributes"]["color"] = color;
            value["Attributes"]["dir"] = dir;
        }
        lightArray.PushBack(value, allocator);
    }
    doc.AddMember("Lights", lightArray, allocator);
    if (!rm->blackHoles.empty()) {
        rapidjson::Value holes(rapidjson::kArrayType);
        for (BlackHole *hole : rm->blackHoles) {
            rapidjson::Value value;
            hole->save(value, allocator);
            holes.PushBack(value, allocator);
        }
        doc.AddMember("BlackHoles", holes, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 1);
    doc.Accept(writer);
    return buffer.GetString();
}

void EngineCore::reloadMap(const std::string &path) {
    FILE *file = fopen(path.c_str(), "r");
    if (file == NULL) {
        std::cout << "Nothing to reload: " << path << " does not exist (save with F5 first)" << std::endl;
        return;
    }
    fclose(file);
    std::cout << "Reloading " << path << std::endl;
    fflush(stdout);
    /* restart the engine on the saved map (simplest way to rebuild every system) */
    char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) {
        std::cout << "Could not restart; run: ./ge " << path << std::endl;
        return;
    }
    exe[n] = 0;
    setenv("GE_SKIP_MENU", "1", 1);
    execl(exe, exe, path.c_str(), (char *) NULL);
    std::cout << "Could not restart; run: ./ge " << path << std::endl;
}

std::string EngineCore::dumpJson() {
    return mapJson();
}

void EngineCore::findPathToPlayer() {
    // temporary debugging function

    // find player idx
    std::size_t playerIdx = 0, enemyIdx = 0;
    const glm::vec3 &playerPosition = player->getTransform().getPosition();
    const glm::vec3 &enemyPosition = enemy->getTransform().getPosition();
    bool foundPlayer = false, foundEnemy = false;

    for (std::size_t u: lastFoundPathToPlayer) {
        RenderComponent *renderComponent = 
                (RenderComponent *) (m_quadTreeEntities[u]->getComponent(Entity::Flags::RENDERABLE));

        renderComponent->getRenderingObject().getMaterials()[0]->setDiffuse(glm::vec3(1.0, 0.0, 1.0));
    }

    playerIdx = PhysicsMaster::getInstance()->findIdx(playerPosition, foundPlayer);
    enemyIdx = PhysicsMaster::getInstance()->findIdx(enemyPosition, foundEnemy);

    if (foundPlayer == true) {
        lastFoundPathToPlayer.clear();

        PhysicsMaster::getInstance()->findShortestPath(enemyIdx, playerIdx, lastFoundPathToPlayer);
        PhysicsMaster::getInstance()->simplifyLastFoundPath(lastFoundPathToPlayer);

        for (std::size_t u: lastFoundPathToPlayer) {
            RenderComponent *renderComponent = 
                (RenderComponent *) (m_quadTreeEntities[u]->getComponent(Entity::Flags::RENDERABLE));

            renderComponent->getRenderingObject().getMaterials()[0]->setDiffuse(glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }
}

void EngineCore::runBenchmark(int numFrames) {
    const int warmupFrames = 30;
    std::vector<double> frameMs;
    frameMs.reserve(numFrames);

    /* GE_BENCH_SYNC=1: per-phase timings include GPU time (adds sync overhead) */
    ProfilingTimer::syncGPU = std::getenv("GE_BENCH_SYNC") != NULL;

    /* GE_SCENARIO=walk (default): walk forward while turning, sprint now and then,
     * drop a point light (Z) every 40 frames and a shadow-casting volumetric spot
     * light (Q) every 150 frames. GE_SCENARIO=static: camera stands still. */
    const char *scenarioEnv = std::getenv("GE_SCENARIO");
    const std::string scenario = scenarioEnv ? scenarioEnv : "walk";
    const bool walk = scenario == "walk";
    /* GE_SCENARIO=fireballs: camera stands still and throws a fireball (G) every 25 frames */
    const bool fireballs = scenario == "fireballs";
    /* GE_SCENARIO=boxes: camera stands still and drops a box (left click) every 12 frames */
    const bool boxes = scenario == "boxes";
    /* GE_SCENARIO=jump: walk forward while turning, press space every 40
     * frames, and print the player's height (tests ground contact / jumping) */
    const bool jumpTest = scenario == "jump";
    /* GE_SCENARIO=hills: run (shift) forward while slowly turning, never
     * jumping, printing the player's position (catches launches off slopes) */
    const bool hillsTest = scenario == "hills";
    /* GE_SCENARIO=path: play-test a route. GE_PATH="x,z;x,z;..." waypoints: the
     * player turns towards each and walks (W) until within 10 units, printing
     * its position; GE_PATH_SHOTS=dir saves a screenshot at each waypoint
     * reached (dir/wp<N>.png). Prints REACHED / PATH DONE (or how far it got) */
    const bool pathTest = scenario == "path";
    std::vector<glm::vec2> waypoints;
    if (const char *pathEnv = std::getenv("GE_PATH")) {
        std::string s(pathEnv);
        size_t at = 0;
        while (at < s.size()) {
            size_t end = s.find(';', at);
            std::string item = s.substr(at, end == std::string::npos ? std::string::npos : end - at);
            float x, z;
            if (std::sscanf(item.c_str(), "%f,%f", &x, &z) == 2) waypoints.push_back(glm::vec2(x, z));
            if (end == std::string::npos) break;
            at = end + 1;
        }
    }
    const char *pathShots = std::getenv("GE_PATH_SHOTS");
    size_t waypoint = 0;
    int stuckFrames = 0;
    glm::vec3 lastPathPosition(0.0f);
    const int segment = 100;
    double segmentSum = 0;
    int segmentStartLights = 0;

    /* GE_SEEK=t: the scene t seconds after its start (its timeline, the sky's Moon); GE_HOLD=1:
     * held at that moment, so a moment is measured rather than the stretch after it */
    if (const char *seek = std::getenv("GE_SEEK")) {
        seekScene((float) std::atof(seek));
    }
    if (std::getenv("GE_HOLD") != NULL) {
        RenderingMaster::getInstance()->setKugelblitzTimeScale(0.0f);
        m_timeline.setTimeScale(0.0f);
    }

    /* benchmarks never react to the real mouse/keyboard (clicks would open light editors) */
    inputManager.setScripted(true);
    if (walk) {
        inputManager.setWarpMouse(true); /* camera-look path; real warping is off when scripted */
    }

    isRunning = true;
    auto benchStart = HighResolutionClock::now();
    for (int i = 0; i < warmupFrames + numFrames && isRunning; i++) {
        if (i == warmupFrames) {
            ProfilingTimer::resetAll();
        }
        if (fireballs) {
            inputManager.injectKey(SDLK_g, i % 25 == 5);
        }
        if (pathTest) {
            glm::vec3 p = player->getTransform().getPosition();
            bool walking = false;
            while (waypoint < waypoints.size()) {
                glm::vec2 d = waypoints[waypoint] - glm::vec2(p.x, p.z);
                if (glm::length(d) < 10.0f) {
                    printf("REACHED %zu frame %d at %.1f %.1f %.1f\n", waypoint, i, p.x, p.y, p.z);
                    if (pathShots != NULL) {
                        saveScreenshot(std::string(pathShots) + "/wp" + std::to_string(waypoint) + ".png");
                    }
                    waypoint++;
                    continue;
                }
                Camera *camera = RenderingMaster::getInstance()->getCamera();
                glm::vec3 r = camera->getRotation();
                camera->setRotation(r.x, std::atan2(-d.x, -d.y), r.z);
                walking = true;
                break;
            }
            inputManager.injectKey(SDLK_w, walking);
            if (i % 30 == 0) {
                printf("PLAYER %d %.1f %.1f %.1f wp %zu\n", i, p.x, p.y, p.z, waypoint);
                stuckFrames = (walking && glm::length(p - lastPathPosition) < 3.0f) ? stuckFrames + 30 : 0;
                lastPathPosition = p;
            }
            if (waypoint == waypoints.size() && !waypoints.empty()) {
                printf("PATH DONE frame %d\n", i);
                waypoints.clear();
            }
        }
        if (hillsTest) {
            inputManager.injectKey(SDLK_w, true);
            inputManager.injectKey(SDLK_LSHIFT, true);
            inputManager.injectMouseDelta(glm::vec2(0.7f, 0.0f));
            if (i % 5 == 0) {
                glm::vec3 p = player->getTransform().getPosition();
                printf("PLAYER %d %.1f %.1f %.1f\n", i, p.x, p.y, p.z);
            }
        }
        if (jumpTest) {
            inputManager.injectKey(SDLK_w, true);
            inputManager.injectKey(SDLK_SPACE, i % 40 == 10);
            inputManager.injectMouseDelta(glm::vec2(1.5f, 0.0f));
            if (i % 5 == 0) {
                glm::vec3 p = player->getTransform().getPosition();
                printf("PLAYER %d %.1f %.1f %.1f\n", i, p.x, p.y, p.z);
            }
        }
        if (boxes && i % 12 == 5) {
            /* in front of the camera (a real click drops it at the camera) */
            Camera *camera = RenderingMaster::getInstance()->getCamera();
            glm::vec3 forward = camera->getForward();
            float side = (float) ((i / 12) % 3) - 1.0f;
            spawnBox(camera->getPosition() + forward * 90.0f + camera->getRight() * (side * 28.0f) + glm::vec3(0, 40, 0));
        }
        if (walk) {
            inputManager.injectKey(SDLK_w, true);
            inputManager.injectKey(SDLK_LSHIFT, (i / 200) % 2 == 1);
            inputManager.injectMouseDelta(glm::vec2(4.0f, 1.5f * glm::sin(i * 0.02f)));
            if (i % 40 == 20) {
                inputManager.injectKey(SDLK_z, true);
            } else {
                inputManager.injectKey(SDLK_z, false);
            }
            if (i % 150 == 75) {
                inputManager.injectKey(SDLK_q, true);
            } else {
                inputManager.injectKey(SDLK_q, false);
            }
        }
        auto t0 = HighResolutionClock::now();
        inputManager.update(RenderingMaster::getInstance()->getDisplay());
        applyMouseLook();
        capturePreviousState();
        PT_FromHere("input");
        input();
        PT_ToHere("input");
        PT_FromHere("update");
        update();
        PT_ToHere("update");
        render(1.0f);
        glFinish(); /* include the GPU work in the measured frame time */
        auto t1 = HighResolutionClock::now();
        if (i >= warmupFrames) {
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            frameMs.push_back(ms);
            /* GE_BENCH_SPIKES=ms: every frame slower than that, where the camera was and what took
             * the time in it (with GE_BENCH_SYNC=1 the phases include their GPU time) */
            static const double spikeMs = std::getenv("GE_BENCH_SPIKES") ? std::atof(std::getenv("GE_BENCH_SPIKES")) : 0.0;
            if (spikeMs > 0.0 && ms >= spikeMs) {
                glm::vec3 p = RenderingMaster::getInstance()->getCamera()->getPosition();
                printf("SPIKE frame %d %.1fms at %.0f %.0f %.0f\n", i, ms, p.x, p.y, p.z);
                ProfilingTimer::printFrame(1.0);
            }
            segmentSum += ms;
            int done = i - warmupFrames + 1;
            if (done % segment == 0) {
                int lightsNow = RenderingMaster::getInstance()->getLights().size();
                printf("  frames %4d-%4d lights %2d->%2d avg=%.2fms (%.1f fps)\n",
                       done - segment, done - 1, segmentStartLights, lightsNow,
                       segmentSum / segment, 1000.0 * segment / segmentSum);
                segmentSum = 0;
                segmentStartLights = lightsNow;
            }
        } else {
            segmentStartLights = RenderingMaster::getInstance()->getLights().size();
        }
        ProfilingTimer::nextFrame();
    }
    double totalS = std::chrono::duration<double>(HighResolutionClock::now() - benchStart).count();

    if (!m_screenshotPath.empty()) {
        saveScreenshot(m_screenshotPath);
    }
    if (const char *savePath = std::getenv("GE_BENCH_SAVE")) {
        saveMap(savePath);
    }

    if (frameMs.empty()) {
        return;
    }
    std::vector<double> sorted = frameMs;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0;
    for (double ms : frameMs) sum += ms;
    double avg = sum / frameMs.size();
    printf("BENCH scenario=%s lights=%zu frames=%zu size=%dx%d avg=%.3fms (%.1f fps) median=%.3fms p95=%.3fms p99=%.3fms min=%.3fms max=%.3fms total=%.2fs\n",
           scenario.c_str(), RenderingMaster::getInstance()->getLights().size(),
           frameMs.size(),
           RenderingMaster::getInstance()->getDisplay()->getWidth(),
           RenderingMaster::getInstance()->getDisplay()->getHeight(),
           avg, 1000.0 / avg,
           sorted[sorted.size() / 2],
           sorted[std::min(sorted.size() - 1, sorted.size() * 95 / 100)],
           sorted[std::min(sorted.size() - 1, sorted.size() * 99 / 100)],
           sorted.front(), sorted.back(), totalS);
    printf("Per-phase (%s):\n", ProfilingTimer::syncGPU ? "CPU+GPU, synced" : "CPU submission only");
    ProfilingTimer::printAll();
    fflush(stdout);
}

void EngineCore::takeScreenshot() {
    /* ~/Pictures/Screenshots/ge-YYYYmmdd-HHMMSS.png, from the back buffer before the overlays */
    const char *home = std::getenv("HOME");
    std::string dir = std::string(home != nullptr ? home : ".") + "/Pictures";
    mkdir(dir.c_str(), 0755);
    dir += "/Screenshots";
    mkdir(dir.c_str(), 0755);
    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
    std::string path = dir + "/ge-" + stamp + ".png";

    Display *display = RenderingMaster::getInstance()->getDisplay();
    int w = display->getWidth(), h = display->getHeight();
    std::vector<unsigned char> pixels(w * h * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    /* GL rows are bottom-up; PNG rows are top-down; and opaque */
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < h; y++) {
        memcpy(&flipped[y * w * 4], &pixels[(h - 1 - y) * w * 4], w * 4);
    }
    for (size_t i = 3; i < flipped.size(); i += 4) {
        flipped[i] = 255;
    }
    if (stbi_write_png(path.c_str(), w, h, 4, flipped.data(), w * 4)) {
        std::cout << "Screenshot saved to " << path << std::endl;
    } else {
        std::cout << "Screenshot: could not write " << path << std::endl;
    }
}

void EngineCore::saveScreenshot(const std::string &path) {
    /* Re-render one frame and read it back before the swap. */
    Display *display = RenderingMaster::getInstance()->getDisplay();
    int w = display->getWidth(), h = display->getHeight();
    std::vector<unsigned char> pixels(w * h * 4);

    m_captureBeforeSwap = &pixels;
    render();
    m_captureBeforeSwap = NULL;

    /* GL rows are bottom-up; PNG rows are top-down */
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < h; y++) {
        memcpy(&flipped[y * w * 4], &pixels[(h - 1 - y) * w * 4], w * 4);
    }
    stbi_write_png(path.c_str(), w, h, 4, flipped.data(), w * 4);
    printf("Screenshot saved to %s\n", path.c_str());
}

/* the sky Kugelblitz put at time t of its timeline: t >= 0 is after the last
 * of the moon fell in (the storm), t < 0 during the collapse before it. Its
 * own clock (what the sky animates on) is put there too, so scrubbing moves
 * everything; from there it runs on (unless paused) */
void EngineCore::seekScene(float t) {
    /* everything as it is t seconds after the start, as if played from there: the timeline's
     * clock, and the sky's Moon destruction (it waits collapseDelay, then falls in) */
    t = std::max(t, 0.0f);
    m_timeline.seek(t);
    for (BlackHole *hole : RenderingMaster::getInstance()->blackHoles) {
        if (!hole->sky || !hole->kugelblitz || hole->driven) continue;      /* (driven: the timeline puts it there) */
        float rate = hole->collapseSpeed > 0.0f ? hole->collapseSpeed / 60.0f : 1.0f / 12.0f;
        hole->playTime = t;
        hole->delayElapsed = std::min(t, hole->collapseDelay);
        float collapse = glm::clamp((t - hole->collapseDelay) * rate, 0.0f, 1.0f);
        if (t >= hole->formationTime()) {
            hole->collapse = 1.0f;
            hole->playing = false;
            hole->formedAt = hole->formationTime();
        } else {
            hole->collapse = std::min(collapse, 0.9999f);
            hole->playing = true;
            hole->formedAt = -1.0f;
        }
        hole->previousCollapse = hole->collapse;
    }
}

/* tools/timeline_editor.py's commands, one JSON object a line:
 *   {"cmd": "sequences", "json": [...]}   the timeline replaced (the clock and its sounds carry on)
 *   {"cmd": "mute", "muted": true}        no sound (while the editor is not playing)
 *   {"cmd": "seek", "t": 12.5}            the Kugelblitz put at that time of its timeline
 *   {"cmd": "pause", "paused": true}      its clock stopped / running
 *   {"cmd": "fps", "fps": 15}             how many frames a second it sends
 *   {"cmd": "camera", "yaw": 16.8, "pitch": 10}  where the camera looks (degrees)
 *   {"cmd": "solo", "params": [[0, "fog"]], "sounds": [[0, 3]]}   only these play (each: its sequence,
 *                                         and its name / its index among the sequence's sounds); none: all */
void EngineCore::remoteCommands() {
    for (const std::string &line : m_remote.poll()) {
        rapidjson::Document command;
        if (command.Parse(line.c_str()).HasParseError() || !command.IsObject() || !command.HasMember("cmd") || !command["cmd"].IsString()) {
            std::cout << "Remote: not a command: " << line.substr(0, 80) << std::endl;
            continue;
        }
        std::string cmd = command["cmd"].GetString();
        if (cmd == "sequences" && command.HasMember("json")) {
            m_timeline.replace(command["json"]);
            m_remoteTouched = true;
        } else if (cmd == "mute" && command.HasMember("muted") && command["muted"].IsBool()) {
            AudioMaster *audio = AudioMaster::getInstance();
            if (command["muted"].GetBool()) {
                if (m_remoteVolume < 0.0f) m_remoteVolume = audio->getMasterVolume();
                audio->setMasterVolume(0.0f);
            } else if (m_remoteVolume >= 0.0f) {
                audio->setMasterVolume(m_remoteVolume);
                m_remoteVolume = -1.0f;
            }
        } else if (cmd == "seek" && command.HasMember("t") && command["t"].IsNumber()) {
            seekScene(command["t"].GetFloat());
            m_remoteTouched = true;
        } else if (cmd == "pause" && command.HasMember("paused") && command["paused"].IsBool()) {
            m_remotePaused = command["paused"].GetBool();
            RenderingMaster::getInstance()->setKugelblitzTimeScale(m_remotePaused ? 0.0f : 1.0f);
            m_timeline.setTimeScale(m_remotePaused ? 0.0f : 1.0f);
            AudioMaster::getInstance()->setPaused(m_remotePaused);      /* its sounds held where they are */
        } else if (cmd == "solo") {
            std::set<std::pair<int, std::string>> params;
            std::set<std::pair<int, int>> sounds;
            if (command.HasMember("params") && command["params"].IsArray()) {
                for (const auto &p : command["params"].GetArray()) {
                    if (p.IsArray() && p.Size() == 2 && p[0].IsInt() && p[1].IsString()) params.insert({ p[0].GetInt(), p[1].GetString() });
                }
            }
            if (command.HasMember("sounds") && command["sounds"].IsArray()) {
                for (const auto &p : command["sounds"].GetArray()) {
                    if (p.IsArray() && p.Size() == 2 && p[0].IsInt() && p[1].IsInt()) sounds.insert({ p[0].GetInt(), p[1].GetInt() });
                }
            }
            m_timeline.setSolo(params, sounds, Aftermath::isolatedOff());
            m_remoteTouched = true;
        } else if (cmd == "fps" && command.HasMember("fps") && command["fps"].IsNumber()) {
            m_remoteFrameMs = 1000.0 / glm::clamp(command["fps"].GetDouble(), 1.0, 60.0);
        } else if (cmd == "camera") {
            Camera *camera = RenderingMaster::getInstance()->getCamera();
            glm::vec3 r = camera->getRotation();
            float pitch = command.HasMember("pitch") && command["pitch"].IsNumber() ? glm::radians(command["pitch"].GetFloat()) : r.x;
            float yaw = command.HasMember("yaw") && command["yaw"].IsNumber() ? glm::radians(command["yaw"].GetFloat()) : r.y;
            camera->setRotation(pitch, yaw, r.z);
            m_remoteTouched = true;
        }
    }
}

void EngineCore::setPlayMode(bool play) {
    /* play mode: hide every overlay and the cursor, mouse captured for looking
     * around; leaving it brings the editor back as it was */
    if (play == m_playMode) {
        return;
    }
    m_playMode = play;
    if (m_playMode) {
        m_warpBeforePlayMode = inputManager.getWarpMouse();
        inputManager.setWarpMouse(true);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        inputManager.setWarpMouse(m_warpBeforePlayMode);
        SDL_ShowCursor(showMouse ? SDL_ENABLE : SDL_DISABLE);
    }
}

void EngineCore::startMenuLoop() {
    if (m_benchFrames > 0 || std::getenv("GE_SKIP_MENU") != NULL) {
        return;
    }
    const float MENU_FPS = 1000.0f / 200;
    isOnStartMenu = true;

    auto lastTime = HighResolutionClock::now();
    auto startTime = lastTime;

    inputManager.setWarpMouse(false);

    while (isOnStartMenu == true) {
        auto lastTime = HighResolutionClock::now();
        unsigned int passedTime = std::chrono::duration_cast<std::chrono::milliseconds>(lastTime - startTime).count();

        if (passedTime < MENU_FPS) {
            SDL_Delay(MENU_FPS - passedTime);
        }

        startTime = lastTime;
        inputManager.update(RenderingMaster::getInstance()->getDisplay());

        if (currentMenu != NULL) {
            currentMenu->input(inputManager);
            currentMenu->update();

            RenderingMaster::getInstance()->clearScreen(0, 0, 0, 1, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            currentMenu->render();
            RenderingMaster::getInstance()->swapBuffers();
        }
    }
}

EngineCore::~EngineCore() {
    if (m_recorder.isRecording()) {
        /* quitting while recording: keep the sound too */
        m_recorder.stop(AudioMaster::getInstance()->stopCapture(), AudioMaster::sampleRate());
    }
    for (auto entity : entities) {
        delete entity;
    }

    RenderingMaster::destroy();
    PhysicsMaster::destroy();
}
