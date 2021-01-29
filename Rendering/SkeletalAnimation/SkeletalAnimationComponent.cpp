#include "SkeletalAnimationComponent.hpp"

#include "Skeleton.hpp"

SkeletalAnimationComponent::SkeletalAnimationComponent(RenderingObject &&renderingObject): 
                                                        renderingObject(std::move(renderingObject)) {
    bool hasTexture = false;
    bool hasNormalMap = false;
    bool hasRoughness = false;

    Skeleton *skeleton = renderingObject.getSkeleton();

    if (skeleton != NULL) {
        static SkeletalAnimation Anim_Test_Idle("Idle", SkeletalAnimation::framesToTime(glm::vec2(51,110)), 2);
        skeleton->setIdleAnimation(&Anim_Test_Idle);
    }
}

void SkeletalAnimationComponent::input(Input &inputManager) {
    static SkeletalAnimation Anim_Test_Walk("Walk_loop", SkeletalAnimation::framesToTime(glm::vec2(0, 23)), 2);
    if (inputManager.getKeyDown(SDLK_k)) {
        playAnimation(Anim_Test_Walk, true, false);
    }
}

void SkeletalAnimationComponent::update() {

}

void SkeletalAnimationComponent::render(Shader *externShader) {
    bool result = true;

    const std::vector<Mesh *> &m_meshes = renderingObject.getMeshes();
    const std::vector<Material *> &m_materials = renderingObject.getMaterials();
    Skeleton *skeleton = renderingObject.getSkeleton();

    if (skeleton != NULL) {
        // potential bottlenck, maybe move to update and perform once on separate thread
        skeleton->update();
        // next update uniforms
        externShader->bind();
        glUniformMatrix4fv(glGetUniformLocation(externShader->getShaderId(), "gBones"),
                            skeleton->m_boneMats.size(),
                            GL_FALSE,
                            glm::value_ptr(skeleton->m_boneMats[0]));
    }
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

void SkeletalAnimationComponent::render() {
}

const unsigned int SkeletalAnimationComponent::getFlag() const {
    return Entity::Flags::ANIMATED;
}

Shader *SkeletalAnimationComponent::getShader() {
    return NULL;
}

std::string SkeletalAnimationComponent::jsonify() {
    std::string res("");

    // if (renderingObject.getFilePath().size() == 0) {
    //     return res;
    // }

    // res += "\"RenderComponent\":{";
    // res += "\"Mesh\":\"" + renderingObject.getFilePath() + "\"";

    // bool addMat = renderingObject.getAddMaterials();

    // if (addMat == false && renderingObject.getMaterials().size() > 0) {
    //     res += ",\"Material\":{";
    //     if ((renderingObject.getMaterials()[0])->getDiffuseTexture() != NULL) {
    //       res += "\"Texture\":\"" + (renderingObject.getMaterials()[0])->getDiffuseTexture()->getFilePath() + "\",";
    //     }
    //     if ((renderingObject.getMaterials()[0])->getNormalTexture() != NULL) {
    //         res += "\"NormalMapTexture\":\"" + (renderingObject.getMaterials()[0])->getNormalTexture()->getFilePath() + "\",";
    //     }
    //     if ((renderingObject.getMaterials()[0])->getRoughnessTexture() != NULL) {
    //         res += "\"RoughnessTexture\":\"" + (renderingObject.getMaterials()[0])->getRoughnessTexture()->getFilePath() + "\",";
    //     }
    //     glm::vec3 ambient = (renderingObject.getMaterials()[0])->getAmbient();
    //     glm::vec3 diffuse = (renderingObject.getMaterials()[0])->getDiffuse();
    //     glm::vec3 specular = (renderingObject.getMaterials()[0])->getSpecular();
    //     float shine = (renderingObject.getMaterials()[0])->getShininess();
    //     float normalMapStrength = (renderingObject.getMaterials()[0])->getNormalMapStrength();
    //     bool isBlackAlpha = (renderingObject.getMaterials()[0])->getIsBlackAlpha();
    //     bool disableCulling = (renderingObject.getMaterials()[0])->getDisableCulling();

    //     res += "\"ambient\":[" + std::to_string(ambient.x) + ","
    //             + std::to_string(ambient.y) + "," + std::to_string(ambient.z) + "],";
    //     res += "\"diffuse\":[" + std::to_string(diffuse.x) + "," 
    //             + std::to_string(diffuse.y) + "," + std::to_string(diffuse.z) + "],";
    //     res += "\"specular\":[" + std::to_string(specular.x) + "," 
    //             + std::to_string(specular.y) + "," + std::to_string(specular.z) + "],";
    //     if ((renderingObject.getMaterials()[0])->getNormalTexture() != NULL) {
    //         res += "\"normalMapStrength\":" + std::to_string(normalMapStrength) + ",";
    //     }
    //     if ((renderingObject.getMaterials()[0])->getDiffuseTexture() != NULL) {
    //         res += "\"isBlackAlpha\":" + std::string(isBlackAlpha == true ? "true,":"false,");
    //         res += "\"disableCulling\":" + std::string(disableCulling == true ? "true,":"false,");
    //     }
    //     res += "\"shininess\":" + std::to_string(shine) + "}";
    // }

    // res += "}";

    return res;
}

void SkeletalAnimationComponent::addAnimation(SkeletalAnimation &anim) {
    m_animations.push_back(anim);
}

SkeletalAnimation *SkeletalAnimationComponent::findAnimation(const std::string &animName) {
    for (unsigned int i = 0; i < m_animations.size(); i++) {
        if (m_animations[i].m_name.compare(animName) == 0)
            return &m_animations[i];
    }

    return NULL;
}

void SkeletalAnimationComponent::playAnimation(SkeletalAnimation &anim, bool loop, bool resetToStart) {
    Skeleton *skeleton = renderingObject.getSkeleton();

    if (skeleton == NULL)
        return;

    skeleton->playAnimation(anim, loop, resetToStart);
}

void SkeletalAnimationComponent::stopAnimating() {
    Skeleton *skeleton = renderingObject.getSkeleton();

    if (skeleton == NULL)
        return;

    skeleton->stopAnimating();
}

SkeletalAnimationComponent::~SkeletalAnimationComponent()
{
}