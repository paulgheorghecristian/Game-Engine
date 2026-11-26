#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "Input.h"
#include <iostream>
#include "Display.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera
{
    public:
        Camera(glm::vec3 position, float xRot, float yRot, float zRot);
        const glm::mat4 &getViewMatrix();
        void moveUp(float);
        void moveSideways(float);
        void moveForward(float);
        void rotateY(float dy);
        void rotateX(float dx);
        void rotateZ(float dz);
        void setZRot(float dz);
        float getZRot() const { return m_zRot; }
        void invertForward();
        virtual ~Camera();
        void setPosition(const glm::vec3& position);
        /* small extra rotation (pitch, yaw, roll; radians) for camera shake */
        void setShake(const glm::vec3 &angles);
        const glm::vec3 &getPosition();
        const glm::vec3 &getRight();
        const glm::vec3 &getForward();
        const glm::vec3 &getRotation();
        void setRotation (float xRot, float yRot, float zRot);
        void setRotation (const glm::vec3 &rotation);
        glm::vec3 getUp();
    protected:
    private:
        glm::vec3 m_shake = glm::vec3(0.0f);
        glm::vec3 position, rotation, forward, right;
        bool forwardNeedsUpdate, viewMatrixNeedsUpdate;
        void computeForward ();
        void computeViewMatrix ();

        glm::mat4 viewMatrix;
        glm::quat m_localZRot;
        float m_zRot = 0.0f;    /* the head's roll (walking), radians */
};

#endif // CAMERA_H
