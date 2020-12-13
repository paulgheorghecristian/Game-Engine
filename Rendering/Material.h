#ifndef MATERIAL_H
#define MATERIAL_H

#include "glm/glm.hpp"

class Material
{
    public:
        Material(const glm::vec3 &ambient,
                 const glm::vec3 &diffuse,
                 const glm::vec3 &specular,
                 float shininess);
        Material (const Material &material);

        void setAmbient (const glm::vec3 &ambient);
        const glm::vec3 &getAmbient () const;
        void setDiffuse (const glm::vec3 &diffuse);
        const glm::vec3 &getDiffuse () const;
        void setSpecular (const glm::vec3 &specular);
        const glm::vec3 &getSpecular () const;
        void setShininess (float s);
        float getShininess () const;

        virtual ~Material();

    protected:

    private:
        glm::vec3 ambient, diffuse, specular;
        float shininess;
};

#endif // MATERIAL_H
