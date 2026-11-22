#include "BlackHole.h"

#include <algorithm>
#include <string>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "PointLight.h"
#include "RenderingMaster.h"
#include "Shader.h"

#include "imgui.h"

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

BlackHole::BlackHole(const rapidjson::Value &json) :
    position(jsonVec3(json, "position", glm::vec3(0.0f)))
{
    radius = std::max(1.0f, jsonFloat(json, "radius", radius));
    lensing = jsonFloat(json, "lensing", lensing);
    influence = std::max(2.0f, jsonFloat(json, "influence", influence));
    diskInner = jsonFloat(json, "diskInner", diskInner);
    diskOuter = jsonFloat(json, "diskOuter", diskOuter);
    diskColor = jsonVec3(json, "diskColor", diskColor);
    diskBrightness = jsonFloat(json, "diskBrightness", diskBrightness);
    diskOpacity = jsonFloat(json, "diskOpacity", diskOpacity);
    diskSpeed = jsonFloat(json, "diskSpeed", diskSpeed);
    colorShift = jsonFloat(json, "colorShift", colorShift);
    diskTilt = jsonVec2(json, "diskTilt", diskTilt);
    glow = jsonFloat(json, "glow", glow);
    sky = json.HasMember("sky") && json["sky"].IsBool() && json["sky"].GetBool();
    direction = jsonVec3(json, "direction", direction);
    angularRadius = jsonFloat(json, "angularRadius", angularRadius);
    moonStream = jsonFloat(json, "moonStream", moonStream);
    cloudClear = jsonFloat(json, "cloudClear", cloudClear);
    updateLight();
}

BlackHole::BlackHole(const glm::vec3 &position) : position(position)
{
    updateLight();
}

BlackHole::~BlackHole()
{
    if (m_light != nullptr) {
        std::vector<Light *> &lights = RenderingMaster::getInstance()->getLights();
        lights.erase(std::remove(lights.begin(), lights.end(), (Light *) m_light), lights.end());
        delete m_light;
    }
}

/* the disk's light on its surroundings: a point light at the hole */
void BlackHole::updateLight()
{
    float range = radius * diskOuter * 6.0f;
    if (glow <= 0.0f || sky) {      /* a sky object is too far to light the ground */
        if (m_light != nullptr) {
            std::vector<Light *> &lights = RenderingMaster::getInstance()->getLights();
            lights.erase(std::remove(lights.begin(), lights.end(), (Light *) m_light), lights.end());
            delete m_light;
            m_light = nullptr;
        }
        return;
    }
    if (m_light == nullptr) {
        m_light = new PointLight(Transform(position, glm::vec3(0), glm::vec3(range)), diskColor, false);
        m_light->setOwnedByFeature(true);
        RenderingMaster::getInstance()->addLightToScene(m_light);
    }
    m_light->getTransform().setPosition(position);
    m_light->getTransform().setScale(glm::vec3(range));
    m_light->setLightColor(diskColor * glow);
}

glm::vec3 BlackHole::diskNormal() const
{
    glm::mat4 tilt = glm::rotate(glm::mat4(1.0f), glm::radians(diskTilt.x), glm::vec3(1, 0, 0)) *
                     glm::rotate(glm::mat4(1.0f), glm::radians(diskTilt.y), glm::vec3(0, 0, 1));
    return glm::normalize(glm::vec3(tilt * glm::vec4(0, 1, 0, 0)));
}

Shader &BlackHole::getShader()
{
    static Shader shader("res/shaders/blackHole.json");
    return shader;
}

glm::mat4 BlackHole::lensSkyView() const
{
    glm::vec3 d = glm::normalize(direction);
    glm::vec3 up = std::abs(d.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    return glm::lookAt(glm::vec3(0.0f), d, up);
}

glm::mat4 BlackHole::lensSkyProjection() const
{
    return glm::perspective(2.0f * LENS_SKY_HALF_ANGLE, 1.0f, 0.1f, 200.0f);
}

bool BlackHole::skyMayBeVisible(const glm::vec3 &forward, const glm::mat4 &projection) const
{
    float tanX = 1.0f / projection[0][0], tanY = 1.0f / projection[1][1];
    float halfDiagonal = std::atan(std::sqrt(tanX * tanX + tanY * tanY));
    float reach = std::asin(std::min(std::tan(angularRadius) * std::max(influence, diskOuter * 1.05f), 0.95f));
    float limit = std::min(halfDiagonal + reach + 0.05f, 3.14159f);
    return glm::dot(glm::normalize(forward), glm::normalize(direction)) > std::cos(limit);
}

void BlackHole::render(GLuint sourceTexture, GLuint depthTexture, GLuint skyCube, int width, int height,
                       GLuint lensSky)
{
    RenderingMaster *rm = RenderingMaster::getInstance();
    Camera *camera = rm->getCamera();
    const glm::mat4 &projection = rm->getProjectionMatrix();
    const glm::mat4 &view = camera->getViewMatrix();
    const glm::vec3 &eye = camera->getPosition();
    if (sky) {
        /* far away in a fixed direction from the camera, like the moon */
        const float SKY_DISTANCE = 20000.0f;
        position = eye + glm::normalize(direction) * SKY_DISTANCE;
        radius = SKY_DISTANCE * std::tan(angularRadius);
    }
    /* rays are marched inside a sphere that holds the whole disk; the
     * bending itself only reaches `influence` radii (a long disk does not
     * widen the warped region) */
    float lensReach = radius * influence;
    float reach = radius * std::max(influence, diskOuter * 1.05f);
    if (sky) {
        reach = std::min(reach, glm::length(position - eye) * 0.95f);
        lensReach = std::min(lensReach, reach);
    }

    /* only the pixels the sphere of influence can cover (scissor) */
    glm::vec3 toHole = position - eye;
    float distance = glm::length(toHole);
    int x0 = 0, y0 = 0, x1 = width, y1 = height;
    if (distance > reach * 1.05f) {
        glm::vec4 clip = projection * view * glm::vec4(position, 1.0f);
        if (clip.w <= 0.0f && glm::dot(toHole, camera->getForward()) < -reach) {
            return;   /* well behind the camera */
        }
        if (clip.w > 0.0f) {
            glm::vec2 centre = (glm::vec2(clip) / clip.w * 0.5f + 0.5f) * glm::vec2(width, height);
            float angular = std::asin(std::min(reach / distance, 1.0f));
            float pixels = std::tan(angular) * projection[1][1] * 0.5f * height * 1.3f + 2.0f;
            x0 = std::max(0, (int) (centre.x - pixels));
            y0 = std::max(0, (int) (centre.y - pixels));
            x1 = std::min(width, (int) (centre.x + pixels));
            y1 = std::min(height, (int) (centre.y + pixels));
            if (x0 >= x1 || y0 >= y1) {
                return;
            }
        }
    }

    Shader &s = getShader();
    glm::mat4 inverseProjection = glm::inverse(projection);
    glm::mat4 inverseView = glm::inverse(view);
    glm::mat4 viewProjection = projection * view;
    glm::vec3 normal = diskNormal();
    float time = rm->getTime();
    int hasSky = skyCube != 0 ? 1 : 0;
    s.updateUniform("sceneSampler", 0);
    s.updateUniform("depthSampler", 1);
    s.updateUniform("skySampler", 2);
    s.updateUniform("hasSky", hasSky);
    s.updateUniform("inverseProjectionMatrix", (void *) &inverseProjection);
    s.updateUniform("inverseViewMatrix", (void *) &inverseView);
    s.updateUniform("viewProjectionMatrix", (void *) &viewProjection);
    s.updateUniform("projectionMatrix", (void *) &projection);
    s.updateUniform("cameraPosition", (void *) &eye);
    s.updateUniform("holePosition", (void *) &position);
    s.updateUniform("holeRadius", radius);
    s.updateUniform("lensing", lensing);
    s.updateUniform("influenceRadius", lensReach);
    s.updateUniform("boundRadius", reach);
    s.updateUniform("diskNormal", (void *) &normal);
    s.updateUniform("diskInner", diskInner);
    s.updateUniform("diskOuter", diskOuter);
    s.updateUniform("diskColor", (void *) &diskColor);
    s.updateUniform("diskBrightness", diskBrightness);
    s.updateUniform("diskOpacity", diskOpacity);
    s.updateUniform("diskSpeed", diskSpeed);
    s.updateUniform("colorShift", colorShift);
    s.updateUniform("time", time);
    int skySource = sky && lensSky != 0 ? 1 : 0;
    s.updateUniform("skySource", skySource);
    if (skySource) {
        glm::mat4 lensSkyViewProjection = lensSkyProjection() * lensSkyView();
        s.updateUniform("lensSkySampler", 3);
        s.updateUniform("lensSkyViewProjection", (void *) &lensSkyViewProjection);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyCube);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, lensSky);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x0, y0, x1 - x0, y1 - y0);
    s.bind();
    rm->drawScreenQuad();
    s.unbind();
    glDisable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
}

bool BlackHole::renderGUI(int index)
{
    bool keep = true;
    ImGui::PushID(index);
    std::string title = "Black hole " + std::to_string(index + 1);
    if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        changed |= ImGui::Checkbox("In the sky (like the moon)", &sky);
        if (sky) {
            if (ImGui::DragFloat3("Direction", &direction.x, 0.005f)) {
                if (glm::length(direction) < 1e-3f) direction = glm::vec3(0, 1, 0);
            }
            ImGui::SliderFloat("Angular radius", &angularRadius, 0.005f, 0.2f, "%.3f rad");
            ImGui::SliderFloat("Moon stream", &moonStream, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Cloud clearing", &cloudClear, 0.0f, 1.0f, "%.2f");
        } else {
            changed |= ImGui::DragFloat3("Position", &position.x, 1.0f);
            changed |= ImGui::SliderFloat("Radius (horizon)", &radius, 2.0f, 300.0f, "%.0f");
        }
        ImGui::SliderFloat("Lensing", &lensing, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Influence (radii)", &influence, 3.0f, 30.0f, "%.1f");
        changed |= ImGui::SliderFloat("Disk inner (radii)", &diskInner, 1.5f, 10.0f, "%.2f");
        changed |= ImGui::SliderFloat("Disk outer (radii)", &diskOuter, 2.0f, 25.0f, "%.1f");
        changed |= ImGui::ColorEdit3("Disk color", &diskColor.x);
        ImGui::SliderFloat("Disk brightness", &diskBrightness, 0.0f, 12.0f, "%.2f");
        ImGui::SliderFloat("Disk opacity", &diskOpacity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Disk speed", &diskSpeed, -4.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Color shift", &colorShift, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Disk tilt", &diskTilt.x, 0.5f);
        changed |= ImGui::SliderFloat("Glow (light)", &glow, 0.0f, 4.0f, "%.2f");
        if (changed) {
            diskOuter = std::max(diskOuter, diskInner + 0.5f);
            updateLight();
        }
        if (ImGui::Button("Delete")) {
            keep = false;
        }
    }
    ImGui::PopID();
    return keep;
}

static void writeVec3(rapidjson::Value &json, const char *key, const glm::vec3 &v,
                      rapidjson::Document::AllocatorType &allocator) {
    rapidjson::Value array(rapidjson::kArrayType);
    array.PushBack(v.x, allocator).PushBack(v.y, allocator).PushBack(v.z, allocator);
    json.AddMember(rapidjson::StringRef(key), array, allocator);
}

void BlackHole::save(rapidjson::Value &json, rapidjson::Document::AllocatorType &allocator) const
{
    json.SetObject();
    if (sky) {
        json.AddMember("sky", true, allocator);
        writeVec3(json, "direction", glm::normalize(direction), allocator);
        json.AddMember("angularRadius", angularRadius, allocator);
        json.AddMember("moonStream", moonStream, allocator);
        json.AddMember("cloudClear", cloudClear, allocator);
    } else {
        writeVec3(json, "position", position, allocator);
        json.AddMember("radius", radius, allocator);
    }
    json.AddMember("lensing", lensing, allocator);
    json.AddMember("influence", influence, allocator);
    json.AddMember("diskInner", diskInner, allocator);
    json.AddMember("diskOuter", diskOuter, allocator);
    writeVec3(json, "diskColor", diskColor, allocator);
    json.AddMember("diskBrightness", diskBrightness, allocator);
    json.AddMember("diskOpacity", diskOpacity, allocator);
    json.AddMember("diskSpeed", diskSpeed, allocator);
    json.AddMember("colorShift", colorShift, allocator);
    rapidjson::Value tilt(rapidjson::kArrayType);
    tilt.PushBack(diskTilt.x, allocator).PushBack(diskTilt.y, allocator);
    json.AddMember("diskTilt", tilt, allocator);
    json.AddMember("glow", glow, allocator);
}
