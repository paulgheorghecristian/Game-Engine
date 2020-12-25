#include "Material.h"

#include "Texture.h"

Material::Material() {
    m_ambient = glm::vec3(1.0f);
    m_diffuse = glm::vec3(1.0f);
    m_specular = glm::vec3(1.0f);
    m_shininess = 1.0f;
    m_normalMapStrength = 1.0f;

    m_diffuseTexture = NULL;
    m_normalTexture  = NULL;
    m_roughnessTexture = NULL;

    m_isBlackAlpha = false;
    m_disableCulling = false;
}

void Material::setAmbient(const glm::vec3 &ambient) {
    this->m_ambient = ambient;
}

const glm::vec3 &Material::getAmbient() const {
    return m_ambient;
}
void Material::setDiffuse(const glm::vec3 &diffuse) {
    this->m_diffuse = diffuse;
}

const glm::vec3 &Material::getDiffuse() const {
    return m_diffuse;
}

void Material::setSpecular(const glm::vec3 &specular) {
    this->m_specular = specular;
}

const glm::vec3 &Material::getSpecular() const {
    return m_specular;
}

void Material::setShininess(float s) {
    this->m_shininess = s;
}

float Material::getShininess() const {
    return m_shininess;
}

void Material::setNormalMapStrength(float s) {
    m_normalMapStrength = s;
}

float Material::getNormalMapStrength() const {
    return m_normalMapStrength;
}

void Material::setIsBlackAlpha(bool s) {
    m_isBlackAlpha = s;
}

bool Material::getIsBlackAlpha() const {
    return m_isBlackAlpha;
}

void Material::setDisableCulling(bool s) {
    m_disableCulling = s;
}

bool Material::getDisableCulling() const {
    return m_disableCulling;
}

Material &Material::operator=(Material &&otherMaterial) {
    if (this != &otherMaterial) {
        m_ambient = otherMaterial.m_ambient;
        m_diffuse = otherMaterial.m_diffuse;
        m_specular = otherMaterial.m_specular;
        m_shininess = otherMaterial.m_shininess;

        delete m_diffuseTexture;
        delete m_normalTexture;
        delete m_roughnessTexture;

        m_diffuseTexture = otherMaterial.m_diffuseTexture;
        m_normalTexture = otherMaterial.m_normalTexture;
        m_roughnessTexture = otherMaterial.m_roughnessTexture;

        otherMaterial.m_diffuseTexture = NULL;
        otherMaterial.m_normalTexture = NULL;
        otherMaterial.m_roughnessTexture = NULL;
    }

    return *this;
}

Material::Material(Material &&otherMaterial) {
    m_ambient = otherMaterial.m_ambient;
    m_diffuse = otherMaterial.m_diffuse;
    m_specular = otherMaterial.m_specular;
    m_shininess = otherMaterial.m_shininess;

    delete m_diffuseTexture;
    delete m_normalTexture;
    delete m_roughnessTexture;

    m_diffuseTexture = otherMaterial.m_diffuseTexture;
    m_normalTexture = otherMaterial.m_normalTexture;
    m_roughnessTexture = otherMaterial.m_roughnessTexture;

    otherMaterial.m_diffuseTexture = NULL;
    otherMaterial.m_normalTexture = NULL;
    otherMaterial.m_roughnessTexture = NULL;
}

Material::~Material() {
    //TODO delete textures
}
