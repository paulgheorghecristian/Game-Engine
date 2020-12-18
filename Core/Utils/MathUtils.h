#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <cmath>
#include <iostream>

class Camera;
class Mesh;

struct Frustum {
    glm::vec3 nbl, nbr, ntl, ntr, fbl, fbr, ftl, ftr;
    float fovY, fovX;
    glm::vec3 center;
};

class MathUtils
{
    public:
        static void calculateFrustum (Camera *camera,
                                      float near,
                                      float far,
                                      float fov,
                                      float aspect,
                                      Frustum &frustum);
        static void calculateFrustumSurroundingCuboid (Camera *camera,
                                                       Frustum &inputFrustum,
                                                       const glm::vec3 &lightDirection,
                                                       Frustum &outputCuboid,
                                                       glm::mat4 &projectionMatrix,
                                                       glm::mat4 &viewMatrix);
    protected:

    private:
        MathUtils() = default;
        ~MathUtils() = default;
};

#endif // MATHUTILS_H
