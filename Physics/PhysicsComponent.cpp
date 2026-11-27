#include "PhysicsComponent.h"

#include "Common.h"

PhysicsComponent::PhysicsComponent(BoundingBodyType type, glm::vec3 scale,
                                    float mass, bool shouldUpdate, Mesh *mesh) : type(type),
                                                                    boundingBodyScale(scale),
                                                                    bodyMass(mass),
                                                                    m_shouldUpdate(shouldUpdate) {
    /* TODO maybe I want to take scale from entity as default?*/
    if ((type == BoundingBodyType::MESH &&
        mesh == nullptr) ||
        (type != BoundingBodyType::MESH &&
        mesh != nullptr)) {
        std::cerr << "Invalid PhysicsComponent state" << std::endl;
        exit(1);
    }

    if (type == BoundingBodyType::MESH) {
        indices.resize(mesh->getIndices().size());
        vertices.resize(mesh->getVertices().size()*3);

        for (int i = 0; i < mesh->getIndices().size(); i++){
            indices[i] = mesh->getIndices()[i];
        }

        for(int i = 0; i < mesh->getVertices().size()*3; i+=3){
            vertices[i] = btScalar(mesh->getVertices()[i/3].positionCoords.x * scale.x);
            vertices[i+1] = btScalar(mesh->getVertices()[i/3].positionCoords.y * scale.y);
            vertices[i+2] = btScalar(mesh->getVertices()[i/3].positionCoords.z * scale.z);
        }
    }
}

void PhysicsComponent::input(Input &inputManager) {

}

void PhysicsComponent::update() {
    btTransform t;
    btVector3 axis;
    btVector3 origin;
    struct btQuaternionFloatData quatXYZ;

    t = m_rigidBody->getCenterOfMassTransform();
    float mat[16];
    t.getOpenGLMatrix(mat);

    t.getRotation().serializeFloat(quatXYZ);
    origin = t.getOrigin();

    if (!m_disabled && m_shouldUpdate == true) {
        _entity->getTransform().setPosition(glm::vec3(origin.x(), origin.y(), origin.z()));
        /* glm::quat's constructor takes (w, x, y, z); Bullet stores (x, y, z, w) */
        _entity->getTransform().setRotation (glm::quat (quatXYZ.m_floats[3],
                                                        quatXYZ.m_floats[0],
                                                        quatXYZ.m_floats[1],
                                                        quatXYZ.m_floats[2]));
        _entity->getTransform().setModelMatrix(glm::scale(glm::make_mat4(mat), _entity->getTransform().getScale()));
    }
}

void PhysicsComponent::render() {

}

void PhysicsComponent::enable() {
    btCollisionShape *newCollisionShape = NULL;

    m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() &
                              ~btCollisionObject::CF_NO_CONTACT_RESPONSE);

    btTransform t = m_rigidBody->getCenterOfMassTransform();
    const glm::vec3 &pos = _entity->getTransform().getPosition();
    const glm::quat &rotation = _entity->getTransform().getRotation();
    const glm::vec3 &scale = _entity->getTransform().getScale();

    switch (type) {
        case BoundingBodyType::CUBE:
            newCollisionShape = new btBoxShape(btVector3(scale.x / 2.0f,
                                                      scale.y / 2.0f,
                                                      scale.z / 2.0f));
            break;
        case BoundingBodyType::SPHERE:
            assert (scale.x == scale.y);
            assert (scale.y == scale.z);
            newCollisionShape = new btSphereShape(scale.x / 2.0f);
            break;
        case BoundingBodyType::CAPSULE:
            newCollisionShape = new btCapsuleShape(scale.x*2.0f, scale.y*2.0f);
            break;
        default:
            assert (false);
    }

    m_rigidBody->setCollisionShape(newCollisionShape);
    m_shape.reset(newCollisionShape);       /* (the old one went with nothing pointing at it: it leaked) */

    t.setOrigin(btVector3(pos.x, pos.y, pos.z));
    t.setRotation (btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

    m_rigidBody->setCenterOfMassTransform(t);
    m_rigidBody->activate(true);

    PhysicsMaster::getInstance()->getWorld()->addRigidBody(m_rigidBody.get());

    Component::enable();
}

void PhysicsComponent::disable() {
    PhysicsMaster::getInstance()->getWorld()->removeRigidBody(m_rigidBody.get());
    m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() |
                              btCollisionObject::CF_NO_CONTACT_RESPONSE);

    Component::disable();
}

void PhysicsComponent::init() {
    assert (_entity != NULL);

    btTransform t;
    btVector3 inertia(0, 0, 0);
    const glm::vec3 &position = _entity->getTransform().getPosition();
    const glm::quat &rotation = _entity->getTransform().getRotation();
    btCollisionShape *collisionShape = nullptr;

    t.setIdentity();
    t.setOrigin (btVector3(position.x, position.y, position.z));
    t.setRotation (btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

    switch (type) {
        case BoundingBodyType::CUBE:
            collisionShape = new btBoxShape(btVector3(boundingBodyScale.x / 2.0f,
                                                      boundingBodyScale.y / 2.0f,
                                                      boundingBodyScale.z / 2.0f));
            break;
        case BoundingBodyType::SPHERE:
            assert (boundingBodyScale.x == boundingBodyScale.y);
            assert (boundingBodyScale.y == boundingBodyScale.z);
            collisionShape = new btSphereShape(boundingBodyScale.x / 2.0f);
            break;
        case BoundingBodyType::CAPSULE:
            collisionShape = new btCapsuleShape(boundingBodyScale.x*2.0f, boundingBodyScale.y*2.0f);
            break;
        case BoundingBodyType::MESH:
            if (m_boundingMeshData == nullptr) {
                m_boundingMeshData.reset(new btTriangleIndexVertexArray(indices.size() / 3,
                                                                        indices.data(),
                                                                        3 * sizeof(int),
                                                                        vertices.size(),
                                                                        vertices.data(),
                                                                        3 * sizeof(float)));
            }
            collisionShape = new btBvhTriangleMeshShape(m_boundingMeshData.get(), true, true);
            break;
        default:
            assert (false);
    }

    if (bodyMass != 0.0) {
        collisionShape->calculateLocalInertia(btScalar(bodyMass), inertia);
    }
    m_shape.reset(collisionShape);
    /* (the motion state was never deleted with the body: one leaked with every body taken out) */
    m_motion.reset(new btDefaultMotionState(t));
    btRigidBody::btRigidBodyConstructionInfo info(bodyMass, m_motion.get(), m_shape.get(), inertia);
    info.m_restitution = 0.5f;
    info.m_friction = 0.5f;

    m_rigidBody.reset(new btRigidBody(info));
    m_rigidBody->setDamping(btScalar(0.1), btScalar(0.1));

    m_userData.reset(new UserData());
    m_userData->type = PointerType::PHYSICS_BODY;
    m_userData->pointer.entity = _entity;
    m_rigidBody->setUserPointer((void *) m_userData.get());

    PhysicsMaster::getInstance()->getWorld()->addRigidBody(m_rigidBody.get());
}

const unsigned int PhysicsComponent::getFlag() const {
    return Entity::Flags::DYNAMIC;
}

const glm::vec3 &PhysicsComponent::getBoundingBodyScale() {
    return boundingBodyScale;
}

std::string PhysicsComponent::jsonify() {
    std::string res("");

    res += "\"PhysicsComponent\":{";
    res += "\"BoundingBodyType\":";
    if (type == BoundingBodyType::CUBE) {
        res += "[\"cube\"],";
    } else if (type == BoundingBodyType::SPHERE) {
        res += "[\"sphere\"],";
    } else if (type == BoundingBodyType::CAPSULE) {
        res += "[\"capsule\"],";
    } else if (type == BoundingBodyType::MESH) {
        res += "[\"mesh\",\"" + m_meshPath + "\"],";
    }
    res += "\"scale\":[" + std::to_string(boundingBodyScale.x) + ","
            + std::to_string(boundingBodyScale.y) + "," + std::to_string(boundingBodyScale.z) + "],";
    res += "\"mass\":" + std::to_string(bodyMass) + ",";
    res += "\"shouldUpdate\":" + (m_shouldUpdate ? std::string("true") : std::string("false")) + "}";

    return res;
}

PhysicsComponent::~PhysicsComponent() {
    /* out of the world; then its parts are freed by their owners (the body first) */
    if (m_rigidBody) PhysicsMaster::getInstance()->getWorld()->removeRigidBody(m_rigidBody.get());
}
