#ifndef MATERIAL_H
#define MATERIAL_H

#include "glm/glm.hpp"

#include <vector>

class Texture;

class Material
{
    public:
        Material();

        Material &operator=(const Material &otherMaterial) = delete;
        Material(const Material &otherMaterial) = delete;

        Material &operator=(Material &&otherMaterial);
        Material(Material &&otherMaterial);

        void setAmbient(const glm::vec3 &ambient);
        const glm::vec3 &getAmbient () const;
        void setDiffuse(const glm::vec3 &diffuse);
        const glm::vec3 &getDiffuse () const;
        void setSpecular(const glm::vec3 &specular);
        const glm::vec3 &getSpecular () const;
        void setShininess(float s);
        float getShininess() const;

        void setDiffuseTexture(Texture *texture) { m_diffuseTexture = texture; }
        void setNormalTexture(Texture *texture) { m_normalTexture = texture; }
        void setRoughnessTexture(Texture *texture) { m_roughnessTexture = texture; }

        Texture *getDiffuseTexture() { return m_diffuseTexture; }
        Texture *getNormalTexture() { return m_normalTexture; }
        Texture *getRoughnessTexture() { return m_roughnessTexture; }

        virtual ~Material();

    protected:

    private:
        glm::vec3 m_ambient, m_diffuse, m_specular;
        float m_shininess;

        Texture *m_diffuseTexture;
        Texture *m_normalTexture;
        Texture *m_roughnessTexture;
};

#endif // MATERIAL_H
