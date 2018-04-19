#include "MathUtils.h"

#include "Camera.h"
#include "Mesh.h"
#include <vector>
#include <limits>

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

    frustum.nearCenter = (farCenter + nearCenter) / 2.0f;
}

void MathUtils::calculateFrustumSurroundingCuboid (Camera *camera,
                                                   const Frustum &inputFrustum,
                                                   const glm::vec3 &lightDirection, /* don't forget to normalize */
                                                   Frustum &outputCuboid) {
    float minX, maxX, minY, maxY, minZ, maxZ;
    glm::vec3 cameraForward = camera->getForward ();
    float forwardLightDirAngle = glm::orientedAngle (glm::normalize(glm::vec2(cameraForward.x, cameraForward.z)),
                                                     glm::normalize(glm::vec2(lightDirection.x, lightDirection.z)));
    float absAngle = glm::abs (glm::degrees(forwardLightDirAngle));
    float yRotCameraAngle = glm::degrees (camera->getRotation().x);

    glm::vec3 up, down, left, right;
#if 1
    std::cout << "angle=" << glm::degrees(forwardLightDirAngle) << std::endl;
    //std::cout << "fovX=" << inputFrustum.fovX << std::endl;
    //std::cout << "fovY=" << inputFrustum.fovY << std::endl;
    //std::cout << "yRot=" << yRotCameraAngle << std::endl;
#endif
    int yCase = 0;

    glm::quat rotY = glm::angleAxis (-glm::orientedAngle(glm::vec2(0, -1), glm::normalize(glm::vec2(lightDirection.x, lightDirection.z))), glm::vec3(0,1,0));
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
        rotatedPoints[i] = rotY * (points[i]-inputFrustum.nearCenter);
        rotatedPoints[i] = rotatedPoints[i] + inputFrustum.nearCenter;
    }

    minX = std::numeric_limits<float>::max();
    maxX = std::numeric_limits<float>::min();
    minZ = std::numeric_limits<float>::max();
    maxZ = std::numeric_limits<float>::min();

    int minXi, maxXi, minZi, maxZi;

    for (int i = 0; i < 8; i++) {
        if (rotatedPoints[i].x < minX) {
            up = points[i];
            minX = rotatedPoints[i].x;
        }
        if (rotatedPoints[i].x > maxX) {
            down = points[i];
            maxX = rotatedPoints[i].x;
        }
        if (rotatedPoints[i].z < minZ) {
            left = points[i];
            minZ = rotatedPoints[i].z;
        }
        if (rotatedPoints[i].z > maxZ) {
            right = points[i];
            maxZ = rotatedPoints[i].z;
        }
    }

    if (yRotCameraAngle <= 90 - inputFrustum.fovY &&
        yRotCameraAngle > inputFrustum.fovY) {
        yCase = 1;
        std::cout << "caz 1 min=NBL max=FTL" << std::endl;
        minY = inputFrustum.nbl.y;
        maxY = inputFrustum.ftl.y;
    } else if (yRotCameraAngle <= inputFrustum.fovY &&
               yRotCameraAngle > -inputFrustum.fovY) {
        yCase = 2;
        std::cout << "caz 2 min=FBL max=FTL" << std::endl;
        minY = inputFrustum.fbl.y;
        maxY = inputFrustum.ftl.y;
    } else {
        yCase = 3;
        std::cout << "caz 3 min=FBL max=NTL" << std::endl;
        minY = inputFrustum.fbl.y;
        maxY = inputFrustum.ntl.y;
    }

/*
    if (absAngle < inputFrustum.fovX) {
        std::cout << "caz1" << std::endl;

        if (forwardLightDirAngle >= 0) {
            right = inputFrustum.ftr;
            if (yCase == 1) {
                left = inputFrustum.ftl;
                up = inputFrustum.fbr;
                down = inputFrustum.ntl;
            } else {
                left = inputFrustum.fbl;
                up = inputFrustum.ftr;
                down = inputFrustum.nbl;
            }
        } else {
            left = inputFrustum.ftl;
            if (yCase == 1) {
                up = inputFrustum.fbl;
                down = inputFrustum.ntr;
                right = inputFrustum.ftr;
            } else {
                up = inputFrustum.ftl;
                down = inputFrustum.nbr;
                right = inputFrustum.fbr;
            }
        }
    } else if (absAngle > inputFrustum.fovX &&
               absAngle <= 90 - inputFrustum.fovX) {
        std::cout << "caz2" << std::endl;
        if (forwardLightDirAngle >= 0) {
            up = inputFrustum.ftr;
            down = inputFrustum.ntl;
            if (yCase == 1) {
                left = inputFrustum.fbl;
                right = inputFrustum.ntr;
            } else {
                left = inputFrustum.ftl;
                right = inputFrustum.nbr;
            }
        } else {
            up = inputFrustum.ftl;
            down = inputFrustum.ntr;
            if (yCase == 1) {
                left = inputFrustum.nbl;
                right = inputFrustum.ftr;
            } else {
                left = inputFrustum.ntl;
                right = inputFrustum.fbr;
            }
        }
    } else if (absAngle > 90 - inputFrustum.fovX &&
               absAngle <= 90) {
        std::cout << "caz3" << std::endl;
        if (forwardLightDirAngle >= 0) {
            up = inputFrustum.ftr;
            down = inputFrustum.ftl;
            left = inputFrustum.ftl;
            right = inputFrustum.ntr;
        } else {
            up = inputFrustum.ftl;
            down = inputFrustum.ftr;
            left = inputFrustum.ntl;
            right = inputFrustum.ftr;
        }
    } else if (absAngle > 90 &&
                absAngle <= 90 + inputFrustum.fovX) {
        std::cout << "caz4" << std::endl;
        if (forwardLightDirAngle >= 0) {
            up = inputFrustum.ftr;
            down = inputFrustum.ftl;
            left = inputFrustum.ftr;
            right = inputFrustum.ntl;
        } else {
            up = inputFrustum.ftl;
            down = inputFrustum.ftr;
            left = inputFrustum.ntr;
            right = inputFrustum.ftl;
        }
    } else if (absAngle > 90 + inputFrustum.fovX &&
                absAngle <= 180 - inputFrustum.fovX) {
        std::cout << "caz5" << std::endl;
        if (forwardLightDirAngle >= 0) {
            up = inputFrustum.ntr;
            down = inputFrustum.ftl;
            left = inputFrustum.ftr;
            right = inputFrustum.ntl;
        } else {
            up = inputFrustum.ntl;
            down = inputFrustum.ftr;
            left = inputFrustum.ntr;
            right = inputFrustum.ftl;
        }
    } else if (absAngle > 180 - inputFrustum.fovX) {
        std::cout << "caz6" << std::endl;
        left = inputFrustum.ftr;
        right = inputFrustum.ftl;

        if (forwardLightDirAngle >= 0) {
            up = inputFrustum.ntr;
            down = inputFrustum.ftl;
        } else {
            up = inputFrustum.ntl;
            down = inputFrustum.ftr;
        }
    }
*/
    glm::vec2 leftLine, rightLine, upLine, downLine;
    float lightDirectionSlope = lightDirection.z / lightDirection.x;

    if (std::fpclassify (lightDirectionSlope) == FP_INFINITE) {
        leftLine.x = lightDirectionSlope;
        leftLine.y = left.x;

        rightLine.x = lightDirectionSlope;
        rightLine.y = right.x;

        upLine.x = 0;
        upLine.y = up.z;

        downLine.x = 0;
        downLine.y = down.z;
    } else if (std::fpclassify (lightDirectionSlope) == FP_NORMAL) {
        leftLine.x = lightDirectionSlope;
        leftLine.y = -lightDirectionSlope*left.x + left.z;

        rightLine.x = lightDirectionSlope;
        rightLine.y = -lightDirectionSlope*right.x + right.z;

        upLine.x = (-1.0f / lightDirectionSlope);
        upLine.y = (1.0f / lightDirectionSlope)*up.x + up.z;

        downLine.x = (-1.0f / lightDirectionSlope);
        downLine.y = (1.0f / lightDirectionSlope)*down.x + down.z;
    } else if (lightDirectionSlope == 0) {
        assert (false);
    } else {
        assert (false);
    }

    glm::vec2 FTL, FTR, NTL, NTR;

    findPointOfIntersection (leftLine, upLine, FTL);
    findPointOfIntersection (upLine, rightLine, FTR);
    findPointOfIntersection (rightLine, downLine, NTR);
    findPointOfIntersection (downLine, leftLine, NTL);

    outputCuboid.ftl = glm::vec3 (FTL.x, maxY, FTL.y);
    outputCuboid.ftr = glm::vec3 (FTR.x, maxY, FTR.y);
    outputCuboid.ntr = glm::vec3 (NTR.x, maxY, NTR.y);
    outputCuboid.ntl = glm::vec3 (NTL.x, maxY, NTL.y);

    outputCuboid.fbl = outputCuboid.ftl;
    outputCuboid.fbl.y = minY;
    outputCuboid.fbr = outputCuboid.ftr;
    outputCuboid.fbr.y = minY;
    outputCuboid.nbr = outputCuboid.ntr;
    outputCuboid.nbr.y = minY;
    outputCuboid.nbl = outputCuboid.ntl;
    outputCuboid.nbl.y = minY;

#if 0
    outputCuboid.fbl = rotatedPoints[0];
    outputCuboid.fbr = rotatedPoints[1];
    outputCuboid.ftl = rotatedPoints[2];
    outputCuboid.ftr = rotatedPoints[3];
    outputCuboid.nbl = rotatedPoints[4];
    outputCuboid.nbr = rotatedPoints[5];
    outputCuboid.ntl = rotatedPoints[6];
    outputCuboid.ntr = rotatedPoints[7];

    outputCuboid.fbl = glm::vec3(minX, minY, minZ);
    outputCuboid.fbr = glm::vec3(maxX, minY, minZ);
    outputCuboid.ftl = glm::vec3(minX, maxY, minZ);
    outputCuboid.ftr = glm::vec3(maxX, maxY, minZ);
    outputCuboid.nbl = glm::vec3(minX, minY, maxZ);
    outputCuboid.nbr = glm::vec3(maxX, minY, maxZ);
    outputCuboid.ntl = glm::vec3(minX, maxY, maxZ);
    outputCuboid.ntr = glm::vec3(maxZ, maxY, maxZ);
#endif
}

void MathUtils::findPointOfIntersection (const glm::vec2 &line1,
                                         const glm::vec2 &line2,
                                         glm::vec2 &pointOfIntersection) {

    if (std::fpclassify (line1.x) == FP_INFINITE) {
        pointOfIntersection.x = line1.y;
        pointOfIntersection.y = line2.x * line1.y + line2.y;
    } else if (std::fpclassify (line2.x) == FP_INFINITE) {
        pointOfIntersection.x = line2.y;
        pointOfIntersection.y = line1.x * line2.y + line1.y;
    } else {
        assert (std::fpclassify (line1.x) == FP_NORMAL &&
                std::fpclassify (line2.x) == FP_NORMAL);

        pointOfIntersection.x = (line2.y - line1.y) / (line1.x - line2.x);
        pointOfIntersection.y = (line1.x*line2.y - line2.x*line1.y) / (line1.x - line2.x);
    }
}

void MathUtils::updateMeshFromCuboid (Mesh *mesh, const Frustum &cuboid) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back (Vertex (cuboid.ntl)); //0
    vertices.push_back (Vertex (cuboid.ntr)); //1
    vertices.push_back (Vertex (cuboid.nbr)); //2
    vertices.push_back (Vertex (cuboid.nbl)); //3

    vertices.push_back (Vertex (cuboid.ftl)); //4
    vertices.push_back (Vertex (cuboid.ftr)); //5
    vertices.push_back (Vertex (cuboid.fbr)); //6
    vertices.push_back (Vertex (cuboid.fbl)); //7

    indices.push_back (0);
    indices.push_back (3);
    indices.push_back (1);
    indices.push_back (3);
    indices.push_back (2);
    indices.push_back (1);

    indices.push_back (5);
    indices.push_back (6);
    indices.push_back (4);
    indices.push_back (6);
    indices.push_back (7);
    indices.push_back (4);

    indices.push_back (2);
    indices.push_back (5);
    indices.push_back (1);
    indices.push_back (2);
    indices.push_back (6);
    indices.push_back (5);

    indices.push_back (4);
    indices.push_back (7);
    indices.push_back (0);
    indices.push_back (7);
    indices.push_back (3);
    indices.push_back (0);

    mesh->update(vertices, indices);
}
