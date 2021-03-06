#include "PhysicsComponent.h"

PhysicsComponent::PhysicsComponent(BoundingBodyType type, glm::vec3 scale, float mass) : type (type),
                                                                                         boundingBodyScale (scale),
                                                                                         bodyMass (mass) {
}

void PhysicsComponent::input(Input &inputManager) {

}

void PhysicsComponent::update() {
    btTransform t;
    btVector3 axis;
    btScalar w;
    btVector3 origin;
    struct btQuaternionFloatData quatXYZ;

    m_rigidBody->getMotionState()->getWorldTransform(t);
    float mat[16];
    t.getOpenGLMatrix(mat);

    t.getRotation().serializeFloat(quatXYZ);
    w = t.getRotation().getW();
    origin = t.getOrigin();

    _entity->getTransform().setPosition(glm::vec3(origin.x(), origin.y(), origin.z()));
    _entity->getTransform().setRotation (glm::quat (quatXYZ.m_floats[0],
                                                    quatXYZ.m_floats[1],
                                                    quatXYZ.m_floats[2],
                                                    quatXYZ.m_floats[3]));
    _entity->getTransform().setModelMatrix(glm::scale(glm::make_mat4(mat), _entity->getTransform().getScale()));
}

void PhysicsComponent::render() {

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

    PhysicsMaster::getWorld()->addRigidBody(m_rigidBody);
}

const unsigned int PhysicsComponent::getFlag() const {
    return Entity::Flags::DYNAMIC;
}

btRigidBody *PhysicsComponent::getRigidBody() {
    return m_rigidBody;
}

PhysicsComponent::~PhysicsComponent() {
    //delete m_rigidBody->getMotionState();
    //delete m_rigidBody;
}
