#include "GrassRenderer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

#include "Material.h"
#include "RenderingMaster.h"
#include "Texture.h"

static float jsonFloat(const rapidjson::Value &json, const char *key, float def) {
    return json.HasMember(key) && json[key].IsNumber() ? json[key].GetFloat() : def;
}

static glm::vec2 jsonVec2(const rapidjson::Value &json, const char *key, const glm::vec2 &def) {
    if (!json.HasMember(key) || !json[key].IsArray() || json[key].Size() < 2) return def;
    return glm::vec2(json[key][0].GetFloat(), json[key][1].GetFloat());
}

/* heightfield resolution (texels per side) */
static const int FIELD_RESOLUTION = 1024;
/* a missing sample (no terrain there): no grass */
static const float NO_GROUND = -1e6f;

GrassRenderer::GrassRenderer(const rapidjson::Value &json, const std::string &terrainMeshPath,
                             const glm::mat4 &terrainModelMatrix, const TerrainLayers *terrainLayers) :
    m_radius(jsonFloat(json, "radius", 110.0f)),
    m_spacing(std::max(0.2f, jsonFloat(json, "spacing", 0.9f))),
    m_width(jsonFloat(json, "width", 0.5f)),
    m_minHeight(jsonFloat(json, "minHeight", -1e5f)),
    m_maxSlope(jsonFloat(json, "maxSlope", 0.45f)),
    m_wind(jsonFloat(json, "wind", 1.0f)),
    m_clumping(jsonFloat(json, "clumping", 0.6f)),
    m_bladeHeight(jsonVec2(json, "height", glm::vec2(2.2f, 4.8f))),
    m_terrain(terrainLayers),
    m_shader("res/shaders/grass.json")
{
    m_grid = std::min(512, (int) std::ceil(2.0f * m_radius / m_spacing));
    if (json.HasMember("clearings") && json["clearings"].IsArray()) {
        for (auto const &c : json["clearings"].GetArray()) {
            if (c.IsArray() && c.Size() >= 3 && m_clearings.size() < 8) {
                m_clearings.push_back(glm::vec3(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat()));
            }
        }
    }

    if (!buildHeightfield(terrainMeshPath, terrainModelMatrix)) {
        std::cout << "Grass: could not build a heightfield from " << terrainMeshPath << std::endl;
        return;
    }

    /* one blade: a tapering strip, x across (-0.5..0.5), y up (0..1) */
    const int segments = 4;
    std::vector<float> blade;
    for (int i = 0; i <= segments; i++) {
        float y = (float) i / segments;
        if (i < segments) {
            blade.push_back(-0.5f); blade.push_back(y);
            blade.push_back(0.5f);  blade.push_back(y);
        } else {
            blade.push_back(0.0f);  blade.push_back(1.0f);   /* pointed tip */
        }
    }
    m_bladeVertices = (int) blade.size() / 2;

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, blade.size() * sizeof(float), blade.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *) 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_shader.updateUniform("heightSampler", 0);
    m_shader.updateUniform("grassSampler", 1);
    m_shader.updateUniform("grass2Sampler", 2);
}

GrassRenderer::~GrassRenderer()
{
    if (m_heightTexture != 0) glDeleteTextures(1, &m_heightTexture);
    if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
    if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
}

/* Rasterizes the terrain mesh (an OBJ, placed by its entity's model matrix)
 * into a top-down height texture. */
bool GrassRenderer::buildHeightfield(const std::string &meshPath, const glm::mat4 &model)
{
    std::ifstream file(meshPath);
    if (!file) {
        return false;
    }
    std::vector<glm::vec3> vertices;
    std::vector<int> triangles;
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() > 2 && line[0] == 'v' && line[1] == ' ') {
            glm::vec3 v;
            if (std::sscanf(line.c_str() + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3) {
                vertices.push_back(glm::vec3(model * glm::vec4(v, 1.0f)));
            }
        } else if (line.size() > 2 && line[0] == 'f' && line[1] == ' ') {
            std::istringstream in(line.substr(2));
            std::string token;
            std::vector<int> face;
            while (in >> token) {
                int index = std::atoi(token.c_str());
                face.push_back(index > 0 ? index - 1 : (int) vertices.size() + index);
            }
            for (size_t i = 1; i + 1 < face.size(); i++) {
                triangles.push_back(face[0]);
                triangles.push_back(face[i]);
                triangles.push_back(face[i + 1]);
            }
        }
    }
    if (vertices.empty() || triangles.empty()) {
        return false;
    }

    glm::vec2 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
    for (const glm::vec3 &v : vertices) {
        lo = glm::min(lo, glm::vec2(v.x, v.z));
        hi = glm::max(hi, glm::vec2(v.x, v.z));
    }
    m_fieldOrigin = lo;
    m_fieldSize = glm::max(hi - lo, glm::vec2(1.0f));
    const int n = FIELD_RESOLUTION;
    m_fieldCell = std::max(m_fieldSize.x, m_fieldSize.y) / (n - 1);
    std::vector<float> heights(n * n, NO_GROUND);

    for (size_t t = 0; t + 2 < triangles.size(); t += 3) {
        const glm::vec3 &a = vertices[triangles[t]], &b = vertices[triangles[t + 1]], &c = vertices[triangles[t + 2]];
        glm::vec2 pa = (glm::vec2(a.x, a.z) - lo) / m_fieldSize * float(n - 1);
        glm::vec2 pb = (glm::vec2(b.x, b.z) - lo) / m_fieldSize * float(n - 1);
        glm::vec2 pc = (glm::vec2(c.x, c.z) - lo) / m_fieldSize * float(n - 1);
        float d = (pb.y - pc.y) * (pa.x - pc.x) + (pc.x - pb.x) * (pa.y - pc.y);
        if (std::abs(d) < 1e-9f) continue;
        int x0 = std::max(0, (int) std::floor(std::min(pa.x, std::min(pb.x, pc.x))));
        int x1 = std::min(n - 1, (int) std::ceil(std::max(pa.x, std::max(pb.x, pc.x))));
        int y0 = std::max(0, (int) std::floor(std::min(pa.y, std::min(pb.y, pc.y))));
        int y1 = std::min(n - 1, (int) std::ceil(std::max(pa.y, std::max(pb.y, pc.y))));
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                float w1 = ((pb.y - pc.y) * (x - pc.x) + (pc.x - pb.x) * (y - pc.y)) / d;
                float w2 = ((pc.y - pa.y) * (x - pc.x) + (pa.x - pc.x) * (y - pc.y)) / d;
                float w3 = 1.0f - w1 - w2;
                if (w1 < -1e-4f || w2 < -1e-4f || w3 < -1e-4f) continue;
                float h = w1 * a.y + w2 * b.y + w3 * c.y;
                float &cell = heights[y * n + x];
                cell = std::max(cell, h);
            }
        }
    }

    glGenTextures(1, &m_heightTexture);
    glBindTexture(GL_TEXTURE_2D, m_heightTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, n, n, 0, GL_RED, GL_FLOAT, heights.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    /* the texture spans a square of side (n - 1) * cell from the origin */
    m_fieldSize = glm::vec2(m_fieldCell * (n - 1));
    return true;
}

void GrassRenderer::render()
{
    if (m_heightTexture == 0 || m_bladeVertices == 0) {
        return;
    }
    RenderingMaster *rm = RenderingMaster::getInstance();
    Camera *camera = rm->getCamera();
    const Environment &env = rm->getEnvironment();

    Shader &s = m_shader;
    s.updateUniform("viewMatrix", (void *) &camera->getViewMatrix());
    s.updateUniform("projectionMatrix", (void *) &rm->getProjectionMatrix());
    s.updateUniform("cameraPosition", (void *) &camera->getPosition());
    s.updateUniform("gridSize", m_grid);
    s.updateUniform("spacing", m_spacing);
    s.updateUniform("radius", m_radius);
    s.updateUniform("bladeHeight", (void *) &m_bladeHeight);
    s.updateUniform("bladeWidth", m_width);
    s.updateUniform("minGroundHeight", m_minHeight);
    s.updateUniform("maxSlope", m_maxSlope);
    s.updateUniform("clumping", m_clumping);
    s.updateUniform("fieldOrigin", (void *) &m_fieldOrigin);
    s.updateUniform("fieldSize", (void *) &m_fieldSize);
    s.updateUniform("fieldCell", m_fieldCell);
    s.updateUniform("windStrength", m_wind);
    rm->uploadWind(s);
    bool pbr = env.pbr;
    s.updateUniform("pbrEnabled", pbr);

    /* the terrain's grass look (so each blade matches the ground under it) */
    float tileSize = 60.0f, shoreHeight = 0.0f, variation = 0.0f;
    bool hasTextures = false;
    if (m_terrain != nullptr && m_terrain->grass != nullptr && m_terrain->grass2 != nullptr) {
        m_terrain->grass->use(1);
        m_terrain->grass2->use(2);
        tileSize = m_terrain->tileSize;
        shoreHeight = m_terrain->shoreHeight;
        variation = m_terrain->variation;
        hasTextures = true;
    }
    s.updateUniform("hasTerrainTextures", hasTextures);
    s.updateUniform("terrainTileSize", tileSize);
    s.updateUniform("terrainShoreHeight", shoreHeight);
    s.updateUniform("terrainVariation", variation);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_heightTexture);

    glDisable(GL_CULL_FACE);   /* blades are seen from both sides */
    int numClearings = (int) m_clearings.size();
    s.updateUniform("numClearings", numClearings);
    s.bind();
    GLint location = glGetUniformLocation(s.getShaderId(), "clearings");
    if (location >= 0 && numClearings > 0) {
        glUniform3fv(location, numClearings, &m_clearings[0].x);
    }
    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, m_bladeVertices, m_grid * m_grid);
    glBindVertexArray(0);
    s.unbind();
    glEnable(GL_CULL_FACE);
}
