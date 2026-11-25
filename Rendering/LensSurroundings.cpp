#include "LensSurroundings.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

#include "BlackHole.h"
#include "Camera.h"
#include "Entity.h"
#include "InstanceRenderComponent.h"
#include "MathUtils.h"
#include "RenderComponent.h"
#include "RenderingMaster.h"
#include "WaterRenderer.h"

/* the faces in GL's order (+x, -x, +y, -y, +z, -z), each looked at the way a cube map is read */
static const glm::vec3 FACE_DIRECTIONS[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
static const glm::vec3 FACE_UPS[6] = { {0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0} };

LensSurroundings::LensSurroundings() :
    m_shader("res/shaders/waterReflection.json")   /* the water's reflection pass: simply lit, fogged */
{
    m_shader.updateUniform("textureSampler", 0);
    float noClip = -1e30f;   /* (it clips at the water; clipping stays off here anyway) */
    m_shader.updateUniform("clipHeight", noClip);
}

LensSurroundings::~LensSurroundings()
{
    if (m_color != 0) glDeleteTextures(1, &m_color);
    if (m_depth != 0) glDeleteTextures(1, &m_depth);
    if (m_fbo != 0) glDeleteFramebuffers(1, &m_fbo);
}

bool LensSurroundings::render(const std::vector<Entity *> &entities, const std::vector<BlackHole *> &holes)
{
    m_faces = 0;
    RenderingMaster *rm = RenderingMaster::getInstance();
    Camera *camera = rm->getCamera();
    const glm::vec3 eye = camera->getPosition();
    const ViewPlanes screen(rm->getProjectionMatrix() * camera->getViewMatrix());

    /* the faces a hole's lens can need: those the cone of its sphere (seen from here) reaches,
     * widened by how far the rays it bends turn before they meet the land past it */
    const float FACE_HALF_DIAGONAL = 0.9553f;   /* a face's corner from its axis: atan(sqrt(2)) */
    const float TURN = glm::radians(30.0f);
    int faces = 0;
    for (BlackHole *hole : holes) {
        if (hole->sky || !hole->drawsAnything()) {
            continue;
        }
        float reach = hole->reach();
        if (!screen.sees(hole->position, reach)) {
            continue;                     /* its lens is nowhere on screen */
        }
        glm::vec3 toHole = hole->position - eye;
        float distance = glm::length(toHole);
        if (distance <= reach * 1.05f) {
            faces = 63;                   /* inside it: rays go every way */
            break;
        }
        /* a physical hole on screen: close round its shadow rays turn by up to 180 degrees, any
         * way, and show what they reach there (an older one darkens them instead) */
        float turning = hole->radius * hole->horizon * 4.0f;
        if (hole->physical && screen.sees(hole->position, turning)) {
            faces = 63;
            break;
        }
        float cone = std::asin(reach / distance) + TURN;
        for (int face = 0; face < 6; face++) {
            float angle = std::acos(glm::clamp(glm::dot(toHole / distance, FACE_DIRECTIONS[face]), -1.0f, 1.0f));
            if (angle < cone + FACE_HALF_DIAGONAL) {
                faces |= 1 << face;
            }
        }
    }
    if (faces == 0) {
        return false;
    }

    if (m_color == 0) {
        glGenTextures(1, &m_color);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_color);
        for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA16F, SIZE, SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        glGenTextures(1, &m_depth);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_depth);
        for (int face = 0; face < 6; face++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT32F, SIZE, SIZE, 0,
                         GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glGenFramebuffers(1, &m_fbo);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }

    m_center = eye;
    m_near = 0.5f;
    m_far = rm->getFarPlane();
    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, m_near, m_far);

    Shader &shader = m_shader;
    shader.updateUniform("projectionMatrix", (void *) &projection);
    shader.updateUniform("cameraPosition", (void *) &m_center);
    WaterRenderer::uploadEnvironment(shader);
    WaterRenderer::uploadForwardLights(shader, m_center);
    rm->uploadWind(shader);

    GLint depthFunc, viewport[4];
    GLfloat clearColor[4];
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, SIZE, SIZE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    /* the terrain is single sided, and the cube sees it from every side */
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    const bool culling = RenderingMaster::viewCulling();
    for (int face = 0; face < 6; face++) {
        if (!(faces & (1 << face))) {
            continue;
        }
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_color, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_depth, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 view = glm::lookAt(m_center, m_center + FACE_DIRECTIONS[face], FACE_UPS[face]);
        shader.updateUniform("viewMatrix", (void *) &view);
        const ViewPlanes seen(projection * view);
        for (Entity *entity : entities) {
            if (entity->getComponent(Entity::Flags::BILL) != NULL) {
                continue;
            }
            RenderComponent *rc = (RenderComponent *) entity->getComponent(Entity::Flags::RENDERABLE);
            if (rc != NULL && !rc->isGlass() && !rc->isBeyondDrawDistance(entity->getTransform()) &&
                !(culling && rc->isOutside(seen, entity->getTransform()))) {
                shader.updateUniform("instanced", false);
                rc->render(&shader);
            }
            InstanceRenderComponent *irc = (InstanceRenderComponent *) entity->getComponent(Entity::Flags::INSTANCE_RENDERABLE);
            if (irc != NULL) {
                shader.updateUniform("instanced", true);
                irc->render(&shader, culling ? &seen : nullptr);
            }
        }
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (cull) glEnable(GL_CULL_FACE);
    glDepthFunc(depthFunc);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    /* blurrier levels: rays that came close to a hole read it blurred */
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_color);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    m_faces = faces;
    return true;
}
