#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Transform
{
    public:
        Transform(const glm::vec3 &position,
                  const glm::quat &rotation,
                  const glm::vec3 &scale);
        Transform(const glm::vec3 &position,
                  const glm::vec3 &rotation,
                  const glm::vec3 &scale);
        Transform(Transform &other);
        Transform &operator=(Transform &other);

        //transform = this.transform * ratio + other.transform (1-ratio);
        Transform interpolateWith(const Transform &other, float ratio);

        const glm::vec3& getPosition() const;
        const glm::quat& getRotation() const;
        const glm::vec3& getScale() const;
        const glm::mat4& getModelMatrix();
        void setPosition(const glm::vec3 &position);
        void setRotation(const glm::quat &rotation);
        void setScale(const glm::vec3 &scale);

        virtual ~Transform();
    protected:

    private:
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        glm::mat4 modelMatrix;
        bool modelMatrixNeedsRefresh;

        void refreshModelMatrix();
};

#endif // TRANSFORM_H
