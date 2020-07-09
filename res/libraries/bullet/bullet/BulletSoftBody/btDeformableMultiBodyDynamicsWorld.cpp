/*
 Written by Xuchen Han <xuchenhan2015@u.northwestern.edu>
 
 Bullet Continuous Collision Detection and Physics Library
 Copyright (c) 2019 Google Inc. http://bulletphysics.org
 This software is provided 'as-is', without any express or implied warranty.
 In no event will the authors be held liable for any damages arising from the use of this software.
 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it freely,
 subject to the following restrictions:
 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 */

/* ====== Overview of the Deformable Algorithm ====== */

/*
A single step of the deformable body simulation contains the following main components:
1. Update velocity to a temporary state v_{n+1}^* = v_n + explicit_force * dt / mass, where explicit forces include gravity and elastic forces.
2. Detect collisions between rigid and deformable bodies at position x_{n+1}^* = x_n + dt * v_{n+1}^*.
3. Then velocities of deformable bodies v_{n+1} are solved in
        M(v_{n+1} - v_{n+1}^*) = damping_force * dt / mass,
   by a conjugate gradient solver, where the damping force is implicit and depends on v_{n+1}.
4. Contact constraints are solved as projections as in the paper by Baraff and Witkin https://www.cs.cmu.edu/~baraff/papers/sig98.pdf. Dynamic frictions are treated as a force and added to the rhs of the CG solve, whereas static frictions are treated as constraints similar to contact.
5. Position is updated via x_{n+1} = x_n + dt * v_{n+1}.
6. Apply position correction to prevent numerical drift.

The algorithm also closely resembles the one in http://physbam.stanford.edu/~fedkiw/papers/stanford2008-03.pdf
 */

#include <stdio.h>
#include "btDeformableMultiBodyDynamicsWorld.h"
#include "btDeformableBodySolver.h"
#include "LinearMath/btQuickprof.h"
#include "btSoftBodyInternals.h"
void btDeformableMultiBodyDynamicsWorld::internalSingleStepSimulation(btScalar timeStep)
{
    BT_PROFILE("internalSingleStepSimulation");
    if (0 != m_internalPreTickCallback)
    {
        (*m_internalPreTickCallback)(this, timeStep);
    }
    reinitialize(timeStep);
    // add gravity to velocity of rigid and multi bodys
    applyRigidBodyGravity(timeStep);
    
    ///apply gravity and explicit force to velocity, predict motion
    predictUnconstraintMotion(timeStep);
    
    ///perform collision detection
    btMultiBodyDynamicsWorld::performDiscreteCollisionDetection();
    
    if (m_selfCollision)
    {
        softBodySelfCollision();
    }
    
    btMultiBodyDynamicsWorld::calculateSimulationIslands();
    
    beforeSolverCallbacks(timeStep);
    
    ///solve contact constraints and then deformable bodies momemtum equation
    solveConstraints(timeStep);
    
    afterSolverCallbacks(timeStep);
    
    integrateTransforms(timeStep);
    
    ///update vehicle simulation
    btMultiBodyDynamicsWorld::updateActions(timeStep);
    
    updateActivationState(timeStep);
    // End solver-wise simulation step
    // ///////////////////////////////
}

void btDeformableMultiBodyDynamicsWorld::updateActivationState(btScalar timeStep)
{
    for (int i = 0; i < m_softBodies.size(); i++)
    {
        btSoftBody* psb = m_softBodies[i];
        psb->updateDeactivation(timeStep);
        if (psb->wantsSleeping())
        {
            if (psb->getActivationState() == ACTIVE_TAG)
                psb->setActivationState(WANTS_DEACTIVATION);
            if (psb->getActivationState() == ISLAND_SLEEPING)
            {
                psb->setZeroVelocity();
            }
        }
        else
        {
            if (psb->getActivationState() != DISABLE_DEACTIVATION)
                psb->setActivationState(ACTIVE_TAG);
        }
    }
    btMultiBodyDynamicsWorld::updateActivationState(timeStep);
}


void btDeformableMultiBodyDynamicsWorld::softBodySelfCollision()
{
    m_deformableBodySolver->updateSoftBodies();
    for (int i = 0; i < m_softBodies.size(); i++)
    {
        btSoftBody* psb = m_softBodies[i];
        if (psb->isActive())
        {
            psb->defaultCollisionHandler(psb);
        }
    }
}

void btDeformableMultiBodyDynamicsWorld::integrateTransforms(btScalar timeStep)
{
    BT_PROFILE("integrateTransforms");
    btMultiBodyDynamicsWorld::integrateTransforms(timeStep);
    for (int i = 0; i < m_softBodies.size(); ++i)
    {
        btSoftBody* psb = m_softBodies[i];
        for (int j = 0; j < psb->m_nodes.size(); ++j)
        {
            btSoftBody::Node& node = psb->m_nodes[j];
            btScalar maxDisplacement = psb->getWorldInfo()->m_maxDisplacement;
            btScalar clampDeltaV = maxDisplacement / timeStep;
            for (int c = 0; c < 3; c++)
            {
                if (node.m_v[c] > clampDeltaV)
                {
                    node.m_v[c] = clampDeltaV;
                }
                if (node.m_v[c] < -clampDeltaV)
                {
                    node.m_v[c] = -clampDeltaV;
                }
            }
            node.m_x  =  node.m_x + timeStep * node.m_v;
            node.m_q = node.m_x;
            node.m_vn = node.m_v;
        }
        // enforce anchor constraints
        for (int j = 0; j < psb->m_deformableAnchors.size();++j)
        {
            btSoftBody::DeformableNodeRigidAnchor& a = psb->m_deformableAnchors[j];
            btSoftBody::Node* n = a.m_node;
            n->m_x = a.m_cti.m_colObj->getWorldTransform() * a.m_local;
            
            // update multibody anchor info
            if (a.m_cti.m_colObj->getInternalType() == btCollisionObject::CO_FEATHERSTONE_LINK)
            {
                btMultiBodyLinkCollider* multibodyLinkCol = (btMultiBodyLinkCollider*)btMultiBodyLinkCollider::upcast(a.m_cti.m_colObj);
                if (multibodyLinkCol)
                {
                    btVector3 nrm;
                    const btCollisionShape* shp = multibodyLinkCol->getCollisionShape();
                    const btTransform& wtr = multibodyLinkCol->getWorldTransform();
                    psb->m_worldInfo->m_sparsesdf.Evaluate(
                                                      wtr.invXform(n->m_x),
                                                      shp,
                                                      nrm,
                                                      0);
                    a.m_cti.m_normal = wtr.getBasis() * nrm;
                    btVector3 normal = a.m_cti.m_normal;
                    btVector3 t1 = generateUnitOrthogonalVector(normal);
                    btVector3 t2 = btCross(normal, t1);
                    btMultiBodyJacobianData jacobianData_normal, jacobianData_t1, jacobianData_t2;
                    findJacobian(multibodyLinkCol, jacobianData_normal, a.m_node->m_x, normal);
                    findJacobian(multibodyLinkCol, jacobianData_t1, a.m_node->m_x, t1);
                    findJacobian(multibodyLinkCol, jacobianData_t2, a.m_node->m_x, t2);
            
                    btScalar* J_n = &jacobianData_normal.m_jacobians[0];
                    btScalar* J_t1 = &jacobianData_t1.m_jacobians[0];
                    btScalar* J_t2 = &jacobianData_t2.m_jacobians[0];
                    
                    btScalar* u_n = &jacobianData_normal.m_deltaVelocitiesUnitImpulse[0];
                    btScalar* u_t1 = &jacobianData_t1.m_deltaVelocitiesUnitImpulse[0];
                    btScalar* u_t2 = &jacobianData_t2.m_deltaVelocitiesUnitImpulse[0];
                    
                    btMatrix3x3 rot(normal.getX(), normal.getY(), normal.getZ(),
                                    t1.getX(), t1.getY(), t1.getZ(),
                                    t2.getX(), t2.getY(), t2.getZ()); // world frame to local frame
                    const int ndof = multibodyLinkCol->m_multiBody->getNumDofs() + 6;
                    btMatrix3x3 local_impulse_matrix = (Diagonal(n->m_im) + OuterProduct(J_n, J_t1, J_t2, u_n, u_t1, u_t2, ndof)).inverse();
                    a.m_c0 =  rot.transpose() * local_impulse_matrix * rot;
                    a.jacobianData_normal = jacobianData_normal;
                    a.jacobianData_t1 = jacobianData_t1;
                    a.jacobianData_t2 = jacobianData_t2;
                    a.t1 = t1;
                    a.t2 = t2;
                }
            }
        }
        psb->interpolateRenderMesh();
    }
}

void btDeformableMultiBodyDynamicsWorld::solveConstraints(btScalar timeStep)
{
    // save v_{n+1}^* velocity after explicit forces
    m_deformableBodySolver->backupVelocity();
    
    // set up constraints among multibodies and between multibodies and deformable bodies
    setupConstraints();
    
    // solve contact constraints
    solveContactConstraints();
    
    // set up the directions in which the velocity does not change in the momentum solve
    m_deformableBodySolver->m_objective->m_projection.setProjection();
    
    // for explicit scheme, m_backupVelocity = v_{n+1}^*
    // for implicit scheme, m_backupVelocity = v_n
    // Here, set dv = v_{n+1} - v_n for nodes in contact
    m_deformableBodySolver->setupDeformableSolve(m_implicit);
    
    // At this point, dv should be golden for nodes in contact
    // proceed to solve deformable momentum equation
    m_deformableBodySolver->solveDeformableConstraints(timeStep);
}

void btDeformableMultiBodyDynamicsWorld::setupConstraints()
{
    // set up constraints between multibody and deformable bodies
    m_deformableBodySolver->setConstraints();
    
    // set up constraints among multibodies
    {
        sortConstraints();
        // setup the solver callback
        btMultiBodyConstraint** sortedMultiBodyConstraints = m_sortedMultiBodyConstraints.size() ? &m_sortedMultiBodyConstraints[0] : 0;
        btTypedConstraint** constraintsPtr = getNumConstraints() ? &m_sortedConstraints[0] : 0;
        m_solverMultiBodyIslandCallback->setup(&m_solverInfo, constraintsPtr, m_sortedConstraints.size(), sortedMultiBodyConstraints, m_sortedMultiBodyConstraints.size(), getDebugDrawer());
        
        // build islands
        m_islandManager->buildIslands(getCollisionWorld()->getDispatcher(), getCollisionWorld());
    }
}

void btDeformableMultiBodyDynamicsWorld::sortConstraints()
{
    m_sortedConstraints.resize(m_constraints.size());
    int i;
    for (i = 0; i < getNumConstraints(); i++)
    {
        m_sortedConstraints[i] = m_constraints[i];
    }
    m_sortedConstraints.quickSort(btSortConstraintOnIslandPredicate2());
    
    m_sortedMultiBodyConstraints.resize(m_multiBodyConstraints.size());
    for (i = 0; i < m_multiBodyConstraints.size(); i++)
    {
        m_sortedMultiBodyConstraints[i] = m_multiBodyConstraints[i];
    }
    m_sortedMultiBodyConstraints.quickSort(btSortMultiBodyConstraintOnIslandPredicate());
}
    
    
void btDeformableMultiBodyDynamicsWorld::solveContactConstraints()
{
    // process constraints on each island
    m_islandManager->processIslands(getCollisionWorld()->getDispatcher(), getCollisionWorld(), m_solverMultiBodyIslandCallback);
    
    // process deferred
    m_solverMultiBodyIslandCallback->processConstraints();
    m_constraintSolver->allSolved(m_solverInfo, m_debugDrawer);
    
    // write joint feedback
    {
        for (int i = 0; i < this->m_multiBodies.size(); i++)
        {
            btMultiBody* bod = m_multiBodies[i];
            
            bool isSleeping = false;
            
            if (bod->getBaseCollider() && bod->getBaseCollider()->getActivationState() == ISLAND_SLEEPING)
            {
                isSleeping = true;
            }
            for (int b = 0; b < bod->getNumLinks(); b++)
            {
                if (bod->getLink(b).m_collider && bod->getLink(b).m_collider->getActivationState() == ISLAND_SLEEPING)
                    isSleeping = true;
            }
            
            if (!isSleeping)
            {
                //useless? they get resized in stepVelocities once again (AND DIFFERENTLY)
                m_scratch_r.resize(bod->getNumLinks() + 1);  //multidof? ("Y"s use it and it is used to store qdd)
                m_scratch_v.resize(bod->getNumLinks() + 1);
                m_scratch_m.resize(bod->getNumLinks() + 1);
                
                if (bod->internalNeedsJointFeedback())
                {
                    if (!bod->isUsingRK4Integration())
                    {
                        if (bod->internalNeedsJointFeedback())
                        {
                            bool isConstraintPass = true;
                            bod->computeAccelerationsArticulatedBodyAlgorithmMultiDof(m_solverInfo.m_timeStep, m_scratch_r, m_scratch_v, m_scratch_m, isConstraintPass,
                                                                                      getSolverInfo().m_jointFeedbackInWorldSpace,
                                                                                      getSolverInfo().m_jointFeedbackInJointFrame);
                        }
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < this->m_multiBodies.size(); i++)
    {
        btMultiBody* bod = m_multiBodies[i];
        bod->processDeltaVeeMultiDof2();
    }
}

void btDeformableMultiBodyDynamicsWorld::addSoftBody(btSoftBody* body, int collisionFilterGroup, int collisionFilterMask)
{
    m_softBodies.push_back(body);
    
    // Set the soft body solver that will deal with this body
    // to be the world's solver
    body->setSoftBodySolver(m_deformableBodySolver);
    
    btCollisionWorld::addCollisionObject(body,
                                         collisionFilterGroup,
                                         collisionFilterMask);
}

void btDeformableMultiBodyDynamicsWorld::predictUnconstraintMotion(btScalar timeStep)
{
    BT_PROFILE("predictUnconstraintMotion");
    btMultiBodyDynamicsWorld::predictUnconstraintMotion(timeStep);
    m_deformableBodySolver->predictMotion(timeStep);
}

void btDeformableMultiBodyDynamicsWorld::reinitialize(btScalar timeStep)
{
    m_internalTime += timeStep;
    m_deformableBodySolver->setImplicit(m_implicit);
    m_deformableBodySolver->setLineSearch(m_lineSearch);
    m_deformableBodySolver->reinitialize(m_softBodies, timeStep);
    btDispatcherInfo& dispatchInfo = btMultiBodyDynamicsWorld::getDispatchInfo();
    dispatchInfo.m_timeStep = timeStep;
    dispatchInfo.m_stepCount = 0;
    dispatchInfo.m_debugDraw = btMultiBodyDynamicsWorld::getDebugDrawer();
    btMultiBodyDynamicsWorld::getSolverInfo().m_timeStep = timeStep;
}

void btDeformableMultiBodyDynamicsWorld::applyRigidBodyGravity(btScalar timeStep)
{
    // Gravity is applied in stepSimulation and then cleared here and then applied here and then cleared here again
    // so that 1) gravity is applied to velocity before constraint solve and 2) gravity is applied in each substep
    // when there are multiple substeps
    clearForces();
    clearMultiBodyForces();
    btMultiBodyDynamicsWorld::applyGravity();
    // integrate rigid body gravity
    for (int i = 0; i < m_nonStaticRigidBodies.size(); ++i)
    {
        btRigidBody* rb = m_nonStaticRigidBodies[i];
        rb->integrateVelocities(timeStep);
    }
    
    // integrate multibody gravity
    {
        forwardKinematics();
        clearMultiBodyConstraintForces();
        {
            for (int i = 0; i < this->m_multiBodies.size(); i++)
            {
                btMultiBody* bod = m_multiBodies[i];
                
                bool isSleeping = false;
                
                if (bod->getBaseCollider() && bod->getBaseCollider()->getActivationState() == ISLAND_SLEEPING)
                {
                    isSleeping = true;
                }
                for (int b = 0; b < bod->getNumLinks(); b++)
                {
                    if (bod->getLink(b).m_collider && bod->getLink(b).m_collider->getActivationState() == ISLAND_SLEEPING)
                        isSleeping = true;
                }
                
                if (!isSleeping)
                {
                    m_scratch_r.resize(bod->getNumLinks() + 1);
                    m_scratch_v.resize(bod->getNumLinks() + 1);
                    m_scratch_m.resize(bod->getNumLinks() + 1);
                    bool isConstraintPass = false;
                    {
                        if (!bod->isUsingRK4Integration())
                        {
                            bod->computeAccelerationsArticulatedBodyAlgorithmMultiDof(m_solverInfo.m_timeStep,
                                                                                      m_scratch_r, m_scratch_v, m_scratch_m,isConstraintPass,
                                                                                      getSolverInfo().m_jointFeedbackInWorldSpace,
                                                                                      getSolverInfo().m_jointFeedbackInJointFrame);
                        }
                        else
                        {
                            btAssert(" RK4Integration is not supported" );
                        }
                    }
                }
            }
        }
    }
    clearForces();
    clearMultiBodyForces();
}

void btDeformableMultiBodyDynamicsWorld::beforeSolverCallbacks(btScalar timeStep)
{
    if (0 != m_internalTickCallback)
    {
        (*m_internalTickCallback)(this, timeStep);
    }
    
    if (0 != m_solverCallback)
    {
        (*m_solverCallback)(m_internalTime, this);
    }
}

void btDeformableMultiBodyDynamicsWorld::afterSolverCallbacks(btScalar timeStep)
{
    if (0 != m_solverCallback)
    {
        (*m_solverCallback)(m_internalTime, this);
    }
}

void btDeformableMultiBodyDynamicsWorld::addForce(btSoftBody* psb, btDeformableLagrangianForce* force)
{
    btAlignedObjectArray<btDeformableLagrangianForce*>& forces = m_deformableBodySolver->m_objective->m_lf;
    bool added = false;
    for (int i = 0; i < forces.size(); ++i)
    {
        if (forces[i]->getForceType() == force->getForceType())
        {
            forces[i]->addSoftBody(psb);
            added = true;
            break;
        }
    }
    if (!added)
    {
        force->addSoftBody(psb);
        force->setIndices(m_deformableBodySolver->m_objective->getIndices());
        forces.push_back(force);
    }
}

void btDeformableMultiBodyDynamicsWorld::removeSoftBody(btSoftBody* body)
{
    m_softBodies.remove(body);
    btCollisionWorld::removeCollisionObject(body);
    // force a reinitialize so that node indices get updated.
    m_deformableBodySolver->reinitialize(m_softBodies, btScalar(-1));
}

void btDeformableMultiBodyDynamicsWorld::removeCollisionObject(btCollisionObject* collisionObject)
{
    btSoftBody* body = btSoftBody::upcast(collisionObject);
    if (body)
        removeSoftBody(body);
    else
        btDiscreteDynamicsWorld::removeCollisionObject(collisionObject);
}
