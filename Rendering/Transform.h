#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

/* TODO make this a move constructor and assign. op */
class Transform
{
    public:
        Transform ();
        Transform(const glm::vec3 &position,
                  const glm::quat &rotation,
                  const glm::vec3 &scale);
        Transform(const glm::vec3 &position,
                  const glm::vec3 &rotation,
                  const glm::vec3 &scale);
        Transform(const Transform &other);
        Transform &operator=(const Transform &other);

        //transform = this.transform * ratio + other.transform (1-ratio);
        void interpolateWith(const Transform &other, float ratio);

        const glm::vec3& getPosition() const;
        const glm::quat& getRotation() const;
        const glm::vec3& getScale() const;
        const glm::mat4& getModelMatrix() const;
        void setPosition(const glm::vec3 &position);
        void setRotation(const glm::quat &rotation);
        void setRotation(const glm::vec3 &rotation);
        void setScale(const glm::vec3 &scale);
        void setModelMatrix (const glm::mat4 &modelMatrix);
        void addPosition(float x, float y, float z);
        void addRotation(float x, float y, float z);
        void addScale(float x, float y, float z);

        virtual ~Transform();
    protected:

    private:
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        mutable glm::mat4 modelMatrix;
        mutable bool modelMatrixNeedsRefresh;

        void refreshModelMatrix() const;
};

#endif // TRANSFORM_H
