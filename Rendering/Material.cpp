#include "Material.h"

Material::Material(const glm::vec3 &ambient,
                   const glm::vec3 &diffuse,
                   const glm::vec3 &specular,
                   float shininess) : ambient (ambient),
                                            diffuse (diffuse),
                                            specular (specular),
                                            shininess (shininess) {

}

Material::Material (const Material &material) {
    ambient = material.getAmbient();
    diffuse = material.getDiffuse();
    specular = material.getSpecular();
    shininess = material.getShininess();
}

void Material::setAmbient (const glm::vec3 &ambient) {
    this->ambient = ambient;
}

const glm::vec3 &Material::getAmbient () const {
    return ambient;
}
void Material::setDiffuse (const glm::vec3 &diffuse) {
    this->diffuse = diffuse;
}

const glm::vec3 &Material::getDiffuse () const {
    return diffuse;
}

void Material::setSpecular (const glm::vec3 &specular) {
    this->specular = specular;
}

const glm::vec3 &Material::getSpecular () const {
    return specular;
}

void Material::setShininess (float s) {
    this->shininess = s;
}

const float &Material::getShininess () const {
    return shininess;
}

Material::~Material() {
}
