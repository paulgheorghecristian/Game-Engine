#include "RenderComponent.h"

RenderComponent::RenderComponent(Mesh * mesh,
                                 Shader * shader,
                                 Texture *texture,
                                 Texture *normalMapTexture,
                                 Texture *roughness,
                                 const Material &material) : mesh (mesh),
                                                             shader (shader),
                                                             texture (texture),
                                                             normalMapTexture (normalMapTexture),
                                                             roughness(roughness),
                                                             material (material) {

    bool result = true;
    bool hasTexture = false;
    bool hasNormalMap = false;
    bool hasRoughness = false;

    if (shader != NULL) {
        if (texture != NULL) {
            result &= shader->updateUniform ("textureSampler", texture->getTextureUnit());
            hasTexture = true;
        }

        if (normalMapTexture != NULL) {
            result &= shader->updateUniform ("normalMapSampler", normalMapTexture->getTextureUnit());
            hasNormalMap = true;
        }

        if (roughness != NULL) {
            result &= shader->updateUniform ("roughnessSampler", roughness->getTextureUnit());
            hasRoughness = true;
        }

        result &= shader->updateUniform ("projectionMatrix", (void *) &RenderingMaster::getInstance()->getProjectionMatrix());
        result &= shader->updateUniform ("viewMatrix", (void *) &RenderingMaster::getInstance()->getCamera()->getViewMatrix());
        result &= shader->updateUniform ("material.ambient", (void *) &this->material.getAmbient());
        result &= shader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
        result &= shader->updateUniform ("material.specular", (void *) &material.getSpecular());
        result &= shader->updateUniform ("material.shininess", (void *) &material.getShininess());

        result &= shader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= shader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= shader->updateUniform ("hasRoughness", (void *) &hasRoughness);
    }
    //assert (result);
}

void RenderComponent::input(Input &inputManager) {

}

void RenderComponent::update() {

}

void RenderComponent::render (Shader *externShader) {
    bool result = true;

    if (externShader != this->shader) {
        bool hasTexture = false;
        bool hasNormalMap = false;
        bool hasRoughness = false;

        result &= externShader->updateUniform ("material.ambient", (void *) &material.getAmbient());
        result &= externShader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
        result &= externShader->updateUniform ("material.specular", (void *) &material.getSpecular());
        result &= externShader->updateUniform ("material.shininess", (void *) &material.getShininess());

        if (texture != NULL) {
            result &= externShader->updateUniform ("textureSampler", texture->getTextureUnit());
            hasTexture = true;
        }

        if (normalMapTexture != NULL) {
            result &= externShader->updateUniform ("normalMapSampler", normalMapTexture->getTextureUnit());
            hasNormalMap = true;
        }

        if (roughness != NULL) {
            result &= externShader->updateUniform ("roughnessSampler", roughness->getTextureUnit());
            hasRoughness = true;
        }

        result &= externShader->updateUniform ("hasTexture", (void *) &hasTexture);
        result &= externShader->updateUniform ("hasNormalMap", (void *) &hasNormalMap);
        result &= externShader->updateUniform ("hasRoughness", (void *) &hasRoughness);
    }

    result &= externShader->updateUniform ("modelMatrix", (void *) &_entity->getTransform().getModelMatrix());

    externShader->bind ();
    if (texture != NULL) {
        texture->use();
    }
    if (normalMapTexture != NULL) {
        normalMapTexture->use();
    }
    if (roughness != NULL) {
        roughness->use();
    }
    mesh->draw ();
    externShader->unbind ();

    //assert (result);
}

void RenderComponent::render() {
    render (this->shader);
}

const unsigned int RenderComponent::getFlag() const {
    return Entity::Flags::RENDERABLE;
}

Shader *RenderComponent::getShader() {
    return shader;
}

RenderComponent::~RenderComponent()
{
    delete mesh;
    delete shader; /*careful this could be shared amongst multiple render components */
    delete texture;
    delete normalMapTexture;
}
