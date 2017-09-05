#include "RenderComponent.h"

RenderComponent::RenderComponent(Mesh * mesh, Shader * shader, const Material &material) : mesh (mesh),
                                                                                           shader (shader),
                                                                                           material (material) {
}

void RenderComponent::input() {

}

void RenderComponent::update() {
}

void RenderComponent::render() {
    bool result = true;
    result &= shader->updateUniform ("modelMatrix", (void *) &_entity->getTransform().getModelMatrix());
    result &= shader->updateUniform ("material.ambient", (void *) &material.getAmbient());
    result &= shader->updateUniform ("material.diffuse", (void *) &material.getDiffuse());
    result &= shader->updateUniform ("material.specular", (void *) &material.getSpecular());
    result &= shader->updateUniform ("material.shininess", (void *) &material.getShininess());

    shader->bind ();
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
    //delete shader; /*this could be shared amongst multiple render components */
}
