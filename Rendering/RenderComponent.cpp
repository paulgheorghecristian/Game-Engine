#include "RenderComponent.h"

RenderComponent::RenderComponent(Mesh * mesh,
                                 Shader * shader,
                                 Texture *texture,
                                 const Material &material) : mesh (mesh),
                                                             shader (shader),
                                                             texture (texture),
                                                             material (material) {

    glm::mat4 viewMatrix = RenderingMaster::getCamera()->getViewMatrix();
    bool result = true;
    int hasTexture = 0;

    result &= shader->updateUniform ("projectionMatrix", (void *) &RenderingMaster::getProjectionMatrix());
    result &= shader->updateUniform ("viewMatrix", (void *) &viewMatrix);
    result &= shader->updateUniform ("material.ambient", (void *) &material.getAmbient());
    result &= shader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
    result &= shader->updateUniform ("material.specular", (void *) &material.getSpecular());
    result &= shader->updateUniform ("material.shininess", (void *) &material.getShininess());

    if (texture) {
        result &= shader->updateUniform ("textureSampler", (void *) &texture->getTextureUnit());
        hasTexture = 1;
    }

    result &= shader->updateUniform ("hasTexture", (void *) &hasTexture);

    assert (result);
}

void RenderComponent::input() {

}

void RenderComponent::update() {

}

void RenderComponent::render() {
    bool result = true;
    glm::mat4 viewMatrix = RenderingMaster::getCamera()->getViewMatrix();

    result &= shader->updateUniform ("modelMatrix", (void *) &_entity->getTransform().getModelMatrix());
    result &= shader->updateUniform ("viewMatrix", (void *) &viewMatrix);

    shader->bind ();
    if (texture) {
        texture->use();
    }
    mesh->draw ();
    shader->unbind ();

    assert (result);
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
}
