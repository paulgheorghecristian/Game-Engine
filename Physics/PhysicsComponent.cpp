#include "PhysicsComponent.h"

PhysicsComponent::PhysicsComponent(BoundingBodyType type, glm::vec3 scale, float mass) : type (type),
                                                                                         boundingBodyScale (scale),
                                                                                         bodyMass (mass) {
/* TODO maybe I want to take scale from entity as default?*/
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

    if (!m_disabled) {
        _entity->getTransform().setPosition(glm::vec3(origin.x(), origin.y(), origin.z()));
        _entity->getTransform().setRotation (glm::quat (quatXYZ.m_floats[0],
                                                        quatXYZ.m_floats[1],
                                                        quatXYZ.m_floats[2],
                                                        quatXYZ.m_floats[3]));
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

    t.setOrigin(btVector3(pos.x, pos.y, pos.z));
    t.setRotation (btQuaternion(rotation.x, rotation.y, rotation.z, rotation.w));

    m_rigidBody->setCenterOfMassTransform(t);
    m_rigidBody->activate(true);

    PhysicsMaster::getInstance()->getWorld()->addRigidBody(m_rigidBody);

    Component::enable();
}

void PhysicsComponent::disable() {
    PhysicsMaster::getInstance()->getWorld()->removeRigidBody(m_rigidBody);
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
    btCollisionShape *collisionShape;

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
        default:
            assert (false);
    }

    if (bodyMass != 0.0) {
        collisionShape->calculateLocalInertia(btScalar(bodyMass), inertia);
    }
    btMotionState *motion = new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo info(bodyMass, motion, collisionShape, inertia);
    info.m_restitution = 0.5f;
    info.m_friction = 0.5f;

    m_rigidBody = new btRigidBody(info);
    m_rigidBody->setDamping(btScalar(0.1), btScalar(0.1));

    PhysicsMaster::getInstance()->getWorld()->addRigidBody(m_rigidBody);
}

const unsigned int PhysicsComponent::getFlag() const {
    return Entity::Flags::DYNAMIC;
}

btRigidBody *PhysicsComponent::getRigidBody() {
    return m_rigidBody;
}

const glm::vec3 &PhysicsComponent::getBoundingBodyScale() {
    return boundingBodyScale;
}

PhysicsComponent::~PhysicsComponent() {
    //delete m_rigidBody->getMotionState();
    //delete m_rigidBody;
}
