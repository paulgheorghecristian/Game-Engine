#include "RenderComponent.h"

RenderComponent::RenderComponent(RenderingObject &&renderingObject) : renderingObject(std::move(renderingObject)) {
    bool hasTexture = false;
    bool hasNormalMap = false;
    bool hasRoughness = false;
}

void RenderComponent::input(Input &inputManager) {

}

void RenderComponent::update() {

}

void RenderComponent::render(Shader *externShader) {
    bool result = true;

    const std::vector<Mesh *> &m_meshes = renderingObject.getMeshes();
    const std::vector<Material *> &m_materials = renderingObject.getMaterials();

    result &= externShader->updateUniform ("modelMatrix", (void *) &_entity->getTransform().getModelMatrix());

    for (unsigned int i = 0; i < m_meshes.size(); i++) {
        bool hasTexture = false;
        bool hasNormalMap = false;
        bool hasRoughness = false;

        result &= externShader->updateUniform ("material.ambient", (void *) &m_materials[i]->getAmbient());
        result &= externShader->updateUniform ("material.diffuse", (void *) &m_materials[i]->getDiffuse());
        result &= externShader->updateUniform ("material.specular", (void *) &m_materials[i]->getSpecular());
        result &= externShader->updateUniform ("material.shininess", m_materials[i]->getShininess());
        result &= externShader->updateUniform ("material.normalMapStrength", m_materials[i]->getNormalMapStrength());
        result &= externShader->updateUniform ("material.isBlackAlpha", m_materials[i]->getIsBlackAlpha());
        externShader->updateUniform ("material.emissive", m_materials[i]->getEffectiveEmissive());
        externShader->updateUniform ("material.specularStrength", m_materials[i]->getSpecularStrength());
        externShader->updateUniform ("material.roughness", m_materials[i]->getRoughness());
        externShader->updateUniform ("material.metallic", m_materials[i]->getMetallic());

        const TerrainLayers *terrain = m_materials[i]->getTerrain();
        bool terrainEnabled = terrain != nullptr && terrain->grass != nullptr && terrain->rock != nullptr;
        externShader->updateUniform ("terrainEnabled", terrainEnabled);
        if (terrainEnabled) {
            Texture *layers[6] = { terrain->grass, terrain->grass2 ? terrain->grass2 : terrain->grass,
                                   terrain->rock, terrain->rockNormal,
                                   terrain->dirt ? terrain->dirt : terrain->grass,
                                   terrain->soil ? terrain->soil : terrain->grass };
            const char *names[6] = { "terrainGrass", "terrainGrass2", "terrainRock", "terrainRockNormal",
                                     "terrainDirt", "terrainSoil" };
            for (int layer = 0; layer < 6; layer++) {
                externShader->updateUniform (names[layer], 3 + layer);
                if (layers[layer] != nullptr) {
                    layers[layer]->use(3 + layer);
                }
            }
            Texture *normals[3] = { terrain->grassNormal, terrain->dirtNormal, terrain->soilNormal };
            const char *normalNames[3] = { "terrainGrassNormal", "terrainDirtNormal", "terrainSoilNormal" };
            for (int layer = 0; layer < 3; layer++) {
                externShader->updateUniform (normalNames[layer], 9 + layer);
                if (normals[layer] != nullptr) {
                    normals[layer]->use(9 + layer);
                }
            }
            bool hasGroundNormals = terrain->grassNormal != nullptr && terrain->dirtNormal != nullptr &&
                                    terrain->soilNormal != nullptr;
            externShader->updateUniform ("terrainHasGroundNormals", hasGroundNormals);
            externShader->updateUniform ("terrainNormalStrength", terrain->normalStrength);
            bool hasRockNormal = terrain->rockNormal != nullptr;
            externShader->updateUniform ("terrainHasRockNormal", hasRockNormal);
            externShader->updateUniform ("terrainTileSize", terrain->tileSize);
            externShader->updateUniform ("terrainShoreHeight", terrain->shoreHeight);
        }

        if (m_materials[i]->getDiffuseTexture() != NULL) {
            result &= externShader->updateUniform ("textureSampler", m_materials[i]->getDiffuseTexture()->getTextureUnit());
            hasTexture = true;
        }

        if (m_materials[i]->getNormalTexture() != NULL) {
            result &= externShader->updateUniform ("normalMapSampler", m_materials[i]->getNormalTexture()->getTextureUnit());
            hasNormalMap = true;
        }

        if (m_materials[i]->getRoughnessTexture() != NULL) {
            result &= externShader->updateUniform ("roughnessSampler", m_materials[i]->getRoughnessTexture()->getTextureUnit());
            hasRoughness = true;
        }

        result &= externShader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= externShader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= externShader->updateUniform ("hasRoughness", (void *) &hasRoughness);

        if (m_materials[i]->getDiffuseTexture() != NULL) {
            m_materials[i]->getDiffuseTexture()->use();
        }
        if (m_materials[i]->getNormalTexture() != NULL) {
            m_materials[i]->getNormalTexture()->use();
        }
        if (m_materials[i]->getRoughnessTexture() != NULL) {
            m_materials[i]->getRoughnessTexture()->use();
        }
        externShader->bind();
        if (m_materials[i]->getDisableCulling() == true) {
            glDisable(GL_CULL_FACE);
        }
        m_meshes[i]->draw();
        if (m_materials[i]->getDisableCulling() == true) {
            glEnable(GL_CULL_FACE);
        }

        bool addMat = renderingObject.getAddMaterials();
        if (addMat == false) {
            /* only 1 mesh is supported when not adding from mtl */
            break;
        }
    }
    externShader->unbind();

}

void RenderComponent::render() {
}

const unsigned int RenderComponent::getFlag() const {
    return Entity::Flags::RENDERABLE;
}

Shader *RenderComponent::getShader() {
    return NULL;
}

std::string RenderComponent::jsonify() {
    std::string res("");

    if (renderingObject.getFilePath().size() == 0) {
        return res;
    }

    res += "\"RenderComponent\":{";
    res += "\"Mesh\":\"" + renderingObject.getFilePath() + "\"";

    bool addMat = renderingObject.getAddMaterials();

    if (addMat == false && renderingObject.getMaterials().size() > 0) {
        res += ",\"Material\":{";
        if ((renderingObject.getMaterials()[0])->getDiffuseTexture() != NULL) {
          res += "\"Texture\":\"" + (renderingObject.getMaterials()[0])->getDiffuseTexture()->getFilePath() + "\",";
        }
        if ((renderingObject.getMaterials()[0])->getNormalTexture() != NULL) {
            res += "\"NormalMapTexture\":\"" + (renderingObject.getMaterials()[0])->getNormalTexture()->getFilePath() + "\",";
        }
        if ((renderingObject.getMaterials()[0])->getRoughnessTexture() != NULL) {
            res += "\"RoughnessTexture\":\"" + (renderingObject.getMaterials()[0])->getRoughnessTexture()->getFilePath() + "\",";
        }
        glm::vec3 ambient = (renderingObject.getMaterials()[0])->getAmbient();
        glm::vec3 diffuse = (renderingObject.getMaterials()[0])->getDiffuse();
        glm::vec3 specular = (renderingObject.getMaterials()[0])->getSpecular();
        float shine = (renderingObject.getMaterials()[0])->getShininess();
        float normalMapStrength = (renderingObject.getMaterials()[0])->getNormalMapStrength();
        bool isBlackAlpha = (renderingObject.getMaterials()[0])->getIsBlackAlpha();
        bool disableCulling = (renderingObject.getMaterials()[0])->getDisableCulling();

        res += "\"ambient\":[" + std::to_string(ambient.x) + ","
                + std::to_string(ambient.y) + "," + std::to_string(ambient.z) + "],";
        res += "\"diffuse\":[" + std::to_string(diffuse.x) + "," 
                + std::to_string(diffuse.y) + "," + std::to_string(diffuse.z) + "],";
        res += "\"specular\":[" + std::to_string(specular.x) + "," 
                + std::to_string(specular.y) + "," + std::to_string(specular.z) + "],";
        if ((renderingObject.getMaterials()[0])->getNormalTexture() != NULL) {
            res += "\"normalMapStrength\":" + std::to_string(normalMapStrength) + ",";
        }
        if ((renderingObject.getMaterials()[0])->getDiffuseTexture() != NULL) {
            res += "\"isBlackAlpha\":" + std::string(isBlackAlpha == true ? "true,":"false,");
            res += "\"disableCulling\":" + std::string(disableCulling == true ? "true,":"false,");
        }
        res += "\"shininess\":" + std::to_string(shine) + "}";
    }

    res += "}";

    return res;
}

RenderComponent::~RenderComponent()
{
}
