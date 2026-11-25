#include "MathUtils.h"

#include "Camera.h"
#include "Mesh.h"
#include <vector>
#include <limits>

ViewPlanes::ViewPlanes(const glm::mat4 &m)
{
    /* clip space: -w <= x, y, z <= w; each side a plane in world space (Gribb & Hartmann) */
    glm::vec4 row[4];
    for (int i = 0; i < 4; i++) row[i] = glm::vec4(m[0][i], m[1][i], m[2][i], m[3][i]);
    planes[0] = row[3] + row[0]; planes[1] = row[3] - row[0];
    planes[2] = row[3] + row[1]; planes[3] = row[3] - row[1];
    planes[4] = row[3] + row[2]; planes[5] = row[3] - row[2];
    for (glm::vec4 &p : planes) p /= std::max(glm::length(glm::vec3(p)), 1e-12f);
}

bool ViewPlanes::sees(const glm::vec3 &center, float radius) const
{
    for (const glm::vec4 &p : planes) {
        if (glm::dot(glm::vec3(p), center) + p.w < -radius) return false;
    }
    return true;
}

void MathUtils::calculateFrustum(Camera *camera,
                                 float near,
                                 float far,
                                 float fov,
                                 float aspect,
                                 Frustum &frustum){

    glm::vec3 cameraForward = camera->getForward();
    glm::vec3 cameraPosition = camera->getPosition();
    glm::vec3 cameraUp = camera->getUp();
    glm::vec3 cameraRight = camera->getRight();
    glm::vec3 farCenter = cameraForward*far + cameraPosition;
    glm::vec3 nearCenter = cameraForward*near + cameraPosition;

    float fovTan = 2.0f * glm::tan (glm::radians (fov / 2.0f));

    float nearPlaneHeight = fovTan * near;
    float nearPlaneWidth = nearPlaneHeight * aspect;

    float farPlaneHeight = fovTan * far;
    float farPlaneWidth = farPlaneHeight * aspect;

    glm::vec3 cameraUpFPH = cameraUp * (farPlaneHeight / 2.0f);
    glm::vec3 cameraUpNPH = cameraUp * (nearPlaneHeight / 2.0f);
    glm::vec3 cameraRightFPW = cameraRight * (farPlaneWidth / 2.0f);
    glm::vec3 cameraRightNPW = cameraRight * (nearPlaneWidth / 2.0f);

    frustum.fbl = farCenter - cameraUpFPH - cameraRightFPW;
    frustum.fbr = farCenter - cameraUpFPH + cameraRightFPW;
    frustum.ftl = farCenter + cameraUpFPH - cameraRightFPW;
    frustum.ftr = farCenter + cameraUpFPH + cameraRightFPW;

    frustum.nbl = nearCenter - cameraUpNPH - cameraRightNPW;
    frustum.nbr = nearCenter - cameraUpNPH + cameraRightNPW;
    frustum.ntl = nearCenter + cameraUpNPH - cameraRightNPW;
    frustum.ntr = nearCenter + cameraUpNPH + cameraRightNPW;

    frustum.fovY = fov / 2.0f;
    frustum.fovX = glm::degrees (glm::atan (farPlaneWidth, 2.0f * far));

    frustum.center = (farCenter + nearCenter) / 2.0f;
}

void MathUtils::calculateFrustumSurroundingCuboid (Camera *camera,
                                                   Frustum &inputFrustum,
                                                   const glm::vec3 &lightDirection, /* don't forget to normalize */
                                                   Frustum &outputCuboid,
                                                   glm::mat4 &projectionMatrix,
                                                   glm::mat4 &viewMatrix) {
    float minX, maxX, minY, maxY, minZ, maxZ;
    float xRot = glm::asin(lightDirection.y);
    float yRot = glm::atan(lightDirection.x, lightDirection.z);

    /* TODO don't really understand why xRot needn't be negative */
    glm::quat quatRotateX = glm::angleAxis (xRot,
                                     glm::vec3 (1, 0, 0));
    glm::quat quatRotateY = glm::angleAxis (-yRot,
                                     glm::vec3 (0, 1, 0));

    glm::quat allRot = glm::normalize(quatRotateX*quatRotateY);
    glm::vec3 points[8], rotatedPoints[8];

    points[0] = inputFrustum.fbl;
    points[1] = inputFrustum.fbr;
    points[2] = inputFrustum.ftl;
    points[3] = inputFrustum.ftr;
    points[4] = inputFrustum.nbl;
    points[5] = inputFrustum.nbr;
    points[6] = inputFrustum.ntl;
    points[7] = inputFrustum.ntr;

    for (int i = 0; i < 8; i++) {
        rotatedPoints[i] = allRot * (points[i]-inputFrustum.center);
        rotatedPoints[i] = rotatedPoints[i] + inputFrustum.center;
    }

    minX = std::numeric_limits<float>::max();
    maxX = std::numeric_limits<float>::min();
    minZ = std::numeric_limits<float>::max();
    maxZ = std::numeric_limits<float>::min();
    minY = std::numeric_limits<float>::max();
    maxY = std::numeric_limits<float>::min();

    for (unsigned int i = 0; i < 8; i++) {
        if (rotatedPoints[i].x < minX) {
            minX = rotatedPoints[i].x;
        }
        if (rotatedPoints[i].x > maxX) {
            maxX = rotatedPoints[i].x;
        }
        if (rotatedPoints[i].z < minZ) {
            minZ = rotatedPoints[i].z;
        }
        if (rotatedPoints[i].z > maxZ) {
            maxZ = rotatedPoints[i].z;
        }
        if (rotatedPoints[i].y < minY) {
            minY = rotatedPoints[i].y;
        }
        if (rotatedPoints[i].y > maxY) {
            maxY = rotatedPoints[i].y;
        }
    }

    float halfWidth = (maxX - minX) / 2.0;
    float halfHeight = (maxY - minY) / 2.0;
    float halfDepth = (maxZ - minZ) / 2.0;

    /* The box fits the camera's view, but casters between it and the light
     * (tall cliffs, a low sun throwing long shadows) must be in the shadow
     * map too, or their shadows vanish depending on where the camera looks:
     * extend it far towards the light (only depth matters for a directional
     * light: a caster shadowing a point inside the box has the same x, y) */
    const float towardsLight = 3000.0f;
    projectionMatrix = glm::ortho (-halfWidth, halfWidth,
                                   -halfHeight, halfHeight,
                                   -halfDepth - towardsLight, halfDepth + 50.0f);

    viewMatrix = glm::mat4_cast (allRot);
    viewMatrix = glm::translate(viewMatrix, -inputFrustum.center);
}
