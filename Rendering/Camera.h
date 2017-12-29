#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include "input.h"
#include <iostream>
#include "display.h"
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
        void rotateY(float);
        void rotateX(float);
        void invertForward();
        virtual ~Camera();
        void setPosition(const glm::vec3& position);
        const glm::vec3 &getPosition();
        const glm::vec3 &getRight();
        const glm::vec3 &getForward();
        const glm::vec3 &getRotation();
        void setRotation (float xRot, float yRot, float zRot);
        void setRotation (const glm::vec3 &rotation);
        glm::vec3 getUp();
    protected:
    private:
        glm::vec3 position, rotation, forward, right;
        void computeForward();

        glm::mat4 viewMatrix;
};

#endif // CAMERA_H
