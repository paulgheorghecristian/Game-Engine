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

/* what a camera sees, as the six planes of its view volume (from its view-projection matrix,
 * any: a mirrored one too), to test bounding spheres against */
struct ViewPlanes {
    explicit ViewPlanes(const glm::mat4 &viewProjection);
    /* false only when the sphere is wholly outside (true: maybe seen) */
    bool sees(const glm::vec3 &center, float radius) const;
    glm::vec4 planes[6];
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
