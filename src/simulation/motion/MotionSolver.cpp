#include "simulation/motion/MotionSolver.h"

#include "components/BaseComponent.h"

#include <QPointF>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

inline double dot(const QPointF& a, const QPointF& b)
{
    return a.x() * b.x() + a.y() * b.y();
}

inline double lengthSq(const QPointF& v)
{
    return dot(v, v);
}

inline double length(const QPointF& v)
{
    return std::sqrt(lengthSq(v));
}

// Resolve a collision between two balls using impulse method.
void resolveBallBall(MotionBody& a, MotionBody& b)
{
    const QPointF delta = b.pos - a.pos;
    const double  dist  = length(delta);
    const double  minD  = a.radius + b.radius;

    if (dist >= minD || dist < 1e-6)
        return;

    const QPointF normal = delta / dist;
    const double  overlap = minD - dist;

    // Positional correction — split evenly unless one body is fixed.
    if (!a.fixed && !b.fixed) {
        a.pos -= normal * (overlap * 0.5);
        b.pos += normal * (overlap * 0.5);
    } else if (a.fixed) {
        b.pos += normal * overlap;
    } else {
        a.pos -= normal * overlap;
    }

    // Velocity impulse.
    const double e = std::min(a.restitution, b.restitution);
    const QPointF vRel = b.vel - a.vel;
    const double vRelN = dot(vRel, normal);

    if (vRelN > 0.0)
        return;  // already separating

    const double invMassA = a.fixed ? 0.0 : 1.0 / a.mass;
    const double invMassB = b.fixed ? 0.0 : 1.0 / b.mass;
    const double impulseMag = -(1.0 + e) * vRelN / (invMassA + invMassB);

    if (!a.fixed) a.vel -= normal * (impulseMag * invMassA);
    if (!b.fixed) b.vel += normal * (impulseMag * invMassB);
}

// Resolve a collision between a ball and an axis-aligned block.
void resolveBallBlock(MotionBody& ball, MotionBody& block)
{
    // Closest point on the block AABB to the ball centre.
    const double cx = std::clamp(ball.pos.x(),
                                 block.pos.x() - block.halfW,
                                 block.pos.x() + block.halfW);
    const double cy = std::clamp(ball.pos.y(),
                                 block.pos.y() - block.halfH,
                                 block.pos.y() + block.halfH);

    const double dx = ball.pos.x() - cx;
    const double dy = ball.pos.y() - cy;
    const double distSq = dx * dx + dy * dy;

    if (distSq >= ball.radius * ball.radius || distSq < 1e-12)
        return;

    const double dist = std::sqrt(distSq);
    const QPointF normal(dx / dist, dy / dist);
    const double overlap = ball.radius - dist;

    if (!ball.fixed && !block.fixed) {
        ball.pos  += normal * (overlap * 0.5);
        block.pos -= normal * (overlap * 0.5);
    } else if (block.fixed) {
        ball.pos  += normal * overlap;
    } else {
        block.pos -= normal * overlap;
    }

    const double e = std::min(ball.restitution, block.restitution);
    const QPointF vRel = ball.vel - block.vel;
    const double vRelN = dot(vRel, normal);

    if (vRelN > 0.0)
        return;

    const double invMassBall  = ball.fixed  ? 0.0 : 1.0 / ball.mass;
    const double invMassBlock = block.fixed ? 0.0 : 1.0 / block.mass;
    const double impulseMag = -(1.0 + e) * vRelN / (invMassBall + invMassBlock);

    if (!ball.fixed)  ball.vel  += normal * (impulseMag * invMassBall);
    if (!block.fixed) block.vel -= normal * (impulseMag * invMassBlock);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MotionSolver::step  — called once per timer tick
// ---------------------------------------------------------------------------
void MotionSolver::step(MotionDomain& domain, double dt)
{
    if (domain.bodies.isEmpty())
        return;

    accumulateForces(domain);
    integrate(domain, dt);
    resolveCollisions(domain);
    if (domain.hasBoundary)
        enforceBoundary(domain);
    writeBack(domain);
}

// ---------------------------------------------------------------------------
// accumulateForces
// Adds gravity and spring forces to each body's force accumulator.
// ---------------------------------------------------------------------------
void MotionSolver::accumulateForces(MotionDomain& domain)
{
    // Reset accumulators.
    for (auto& b : domain.bodies)
        b.force = QPointF(0.0, 0.0);

    // Gravity acts downward (+Y in Qt scene coordinates).
    for (auto& b : domain.bodies) {
        if (!b.fixed && b.mass > 0.0)
            b.force.ry() += b.mass * domain.gravity;
    }

    // Spring forces (Hooke + linear damping along spring axis).
    for (const auto& sp : domain.springs) {
        const QPointF pA = (sp.bodyA >= 0) ? domain.bodies[sp.bodyA].pos : sp.anchorA;
        const QPointF pB = (sp.bodyB >= 0) ? domain.bodies[sp.bodyB].pos : sp.anchorB;
        const QPointF vA = (sp.bodyA >= 0) ? domain.bodies[sp.bodyA].vel : QPointF();
        const QPointF vB = (sp.bodyB >= 0) ? domain.bodies[sp.bodyB].vel : QPointF();

        const QPointF delta = pB - pA;
        double len = length(delta);
        if (len < 1.0) len = 1.0;  // avoid div-by-zero for coincident points

        const QPointF dir = delta / len;  // unit vector A → B

        // Hooke: F = k * (len - rest)
        const double Fspring = sp.stiffness * (len - sp.restLength);

        // Damping: F_damp = d * (v_rel · dir)
        const double Fdamp = sp.damping * dot(vB - vA, dir);

        const QPointF F = dir * (Fspring + Fdamp);

        if (sp.bodyA >= 0 && !domain.bodies[sp.bodyA].fixed)
            domain.bodies[sp.bodyA].force += F;   // force on A is toward B

        if (sp.bodyB >= 0 && !domain.bodies[sp.bodyB].fixed)
            domain.bodies[sp.bodyB].force -= F;   // reaction on B is toward A
    }
}

// ---------------------------------------------------------------------------
// integrate — symplectic (semi-implicit) Euler
// v_new = v + a·dt          (a = F/m)
// x_new = x + v_new·dt      (uses new velocity → energy-conserving for springs)
// ---------------------------------------------------------------------------
void MotionSolver::integrate(MotionDomain& domain, double dt)
{
    for (auto& b : domain.bodies) {
        if (b.fixed || b.mass <= 0.0)
            continue;
        b.vel += (b.force / b.mass) * dt;
        b.pos += b.vel * dt;
    }
}

// ---------------------------------------------------------------------------
// resolveCollisions — pairwise narrow-phase
// ---------------------------------------------------------------------------
void MotionSolver::resolveCollisions(MotionDomain& domain)
{
    const int n = domain.bodies.size();
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            auto& a = domain.bodies[i];
            auto& b = domain.bodies[j];

            if (a.shape == MotionBody::Shape::Ball && b.shape == MotionBody::Shape::Ball) {
                resolveBallBall(a, b);
            } else if (a.shape == MotionBody::Shape::Ball && b.shape == MotionBody::Shape::Block) {
                resolveBallBlock(a, b);
            } else if (a.shape == MotionBody::Shape::Block && b.shape == MotionBody::Shape::Ball) {
                resolveBallBlock(b, a);
            }
            // Block–block collision not implemented in Phase 3.
        }
    }
}

// ---------------------------------------------------------------------------
// enforceBoundary — reflect velocity off canvas edges
// ---------------------------------------------------------------------------
void MotionSolver::enforceBoundary(MotionDomain& domain)
{
    for (auto& b : domain.bodies) {
        if (b.fixed)
            continue;

        if (b.shape == MotionBody::Shape::Ball) {
            // Left wall
            if (b.pos.x() - b.radius < domain.boundaryLeft) {
                b.pos.rx() = domain.boundaryLeft + b.radius;
                b.vel.rx() =  std::abs(b.vel.x()) * b.restitution;
            }
            // Right wall
            if (b.pos.x() + b.radius > domain.boundaryRight) {
                b.pos.rx() = domain.boundaryRight - b.radius;
                b.vel.rx() = -std::abs(b.vel.x()) * b.restitution;
            }
            // Top wall
            if (b.pos.y() - b.radius < domain.boundaryTop) {
                b.pos.ry() = domain.boundaryTop + b.radius;
                b.vel.ry() =  std::abs(b.vel.y()) * b.restitution;
            }
            // Bottom wall (floor)
            if (b.pos.y() + b.radius > domain.boundaryBottom) {
                b.pos.ry() = domain.boundaryBottom - b.radius;
                b.vel.ry() = -std::abs(b.vel.y()) * b.restitution;
            }
        } else {
            // Block boundary clamping.
            if (b.pos.x() - b.halfW < domain.boundaryLeft) {
                b.pos.rx() = domain.boundaryLeft + b.halfW;
                if (b.vel.x() < 0.0) b.vel.rx() = -b.vel.x() * b.restitution;
            }
            if (b.pos.x() + b.halfW > domain.boundaryRight) {
                b.pos.rx() = domain.boundaryRight - b.halfW;
                if (b.vel.x() > 0.0) b.vel.rx() = -b.vel.x() * b.restitution;
            }
            if (b.pos.y() - b.halfH < domain.boundaryTop) {
                b.pos.ry() = domain.boundaryTop + b.halfH;
                if (b.vel.y() < 0.0) b.vel.ry() = -b.vel.y() * b.restitution;
            }
            if (b.pos.y() + b.halfH > domain.boundaryBottom) {
                b.pos.ry() = domain.boundaryBottom - b.halfH;
                if (b.vel.y() > 0.0) b.vel.ry() = -b.vel.y() * b.restitution;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// writeBack
// Push physics state into the Qt scene.  BaseComponent::itemChange()
// automatically reroutes all wires connected to moved items.
// ---------------------------------------------------------------------------
void MotionSolver::writeBack(MotionDomain& domain)
{
    for (auto& b : domain.bodies) {
        if (b.fixed || b.component == nullptr)
            continue;
        b.component->setPos(b.pos);
        b.component->simState["vx"]    = b.vel.x();
        b.component->simState["vy"]    = b.vel.y();
        b.component->simState["speed"] = std::hypot(b.vel.x(), b.vel.y());
        b.component->update();
    }

    // Update Spring visual endpoints stored in simState so SpringComponent
    // can draw a live stretched spring between the two connected bodies.
    for (const auto& sp : domain.springs) {
        if (sp.springComponent == nullptr)
            continue;
        const QPointF pA = (sp.bodyA >= 0) ? domain.bodies[sp.bodyA].pos : sp.anchorA;
        const QPointF pB = (sp.bodyB >= 0) ? domain.bodies[sp.bodyB].pos : sp.anchorB;
        sp.springComponent->simState["endAx"]           = pA.x();
        sp.springComponent->simState["endAy"]           = pA.y();
        sp.springComponent->simState["endBx"]           = pB.x();
        sp.springComponent->simState["endBy"]           = pB.y();
        sp.springComponent->simState["hasLiveEndpoints"] = true;
        sp.springComponent->update();
    }
}
