#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <glm/glm.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <cmath>

class Camera;
class Mesh;

struct Frustum {
    glm::vec3 nbl, nbr, ntl, ntr, fbl, fbr, ftl, ftr;
    float fovY, fovX;
    glm::vec3 nearCenter;
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
                                                       const Frustum &inputFrustum,
                                                       const glm::vec3 &lightDirection,
                                                       Frustum &outputCuboid);
        static void updateMeshFromCuboid (Mesh *mesh, const Frustum &cuboid);
        static void findPointOfIntersection (const glm::vec2 &line1,
                                             const glm::vec2 &line2,
                                             glm::vec2 &pointOfIntersection);

    protected:

    private:
        MathUtils() = default;
        ~MathUtils() = default;
};

#endif // MATHUTILS_H
