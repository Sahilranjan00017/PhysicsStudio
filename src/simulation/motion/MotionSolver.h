#pragma once

#include "simulation/SolverInterfaces.h"

#include <QList>
#include <QPointF>

class BaseComponent;
class Wire;

// ---------------------------------------------------------------------------
// MotionBody
// Runtime physics state for one Ball or Block component.
// ---------------------------------------------------------------------------
struct MotionBody {
    enum class Shape { Ball, Block };

    BaseComponent* component = nullptr;  // back-pointer; not owned
    Shape          shape     = Shape::Ball;
    bool           fixed     = false;    // true → infinite mass, never integrates

    double mass        = 1.0;   // kg
    double restitution = 0.8;   // coefficient of restitution (0 = perfectly inelastic)

    double radius = 20.0;       // Ball: half-diameter in scene pixels
    double halfW  = 40.0;       // Block: half-width  in scene pixels
    double halfH  = 20.0;       // Block: half-height in scene pixels

    QPointF pos;    // scene centre position (pixels)
    QPointF vel;    // velocity (pixels / second)
    QPointF force;  // accumulated force for current tick (reset each step)
};

// ---------------------------------------------------------------------------
// MotionSpring
// Hooke-damped spring force between two bodies, or body + fixed anchor.
// bodyA / bodyB == -1 means the corresponding endpoint is anchorA / anchorB.
// ---------------------------------------------------------------------------
struct MotionSpring {
    int     bodyA     = -1;     // index into MotionDomain::bodies
    int     bodyB     = -1;     // index into MotionDomain::bodies
    QPointF anchorA;            // world pos when bodyA == -1
    QPointF anchorB;            // world pos when bodyB == -1

    double stiffness  = 200.0;  // N/m  (px·kg/s²)
    double restLength = 100.0;  // pixels
    double damping    =   2.0;  // N·s/m

    BaseComponent* springComponent = nullptr;  // for writing simState
};

// ---------------------------------------------------------------------------
// MotionPendulum
// Simple gravity pendulum with angle-based ODE integration.
// Pivot is fixed in scene space; bob moves on a circular arc.
// ---------------------------------------------------------------------------
struct MotionPendulum {
    BaseComponent* component = nullptr;  // back-pointer; not owned

    QPointF pivot;            // scene-space pivot position (component pos())
    double  length   = 100.0; // arm length in pixels
    double  angle    = 0.3;   // current angle (rad from vertical downward)
    double  omega    = 0.0;   // angular velocity (rad/s)
    double  damping  = 0.05;  // viscous damping coefficient (1/s)
    double  bobRadius = 12.0; // bob visual radius (px)
};

// ---------------------------------------------------------------------------
// MotionRamp
// Inclined line segment used as a one-sided collision surface.
// Balls that penetrate the ramp surface receive a normal impulse.
// ---------------------------------------------------------------------------
struct MotionRamp {
    BaseComponent* component = nullptr;

    QPointF p1;               // scene endpoint A (left end of ramp in local space)
    QPointF p2;               // scene endpoint B (right end of ramp in local space)
    double  restitution = 0.7;
};

// ---------------------------------------------------------------------------
// MotionDomain
// Complete mechanical scene snapshot.  Persists across ticks so velocity
// state is maintained.  Rebuilt from scene on structural changes (not on
// every tick).
// ---------------------------------------------------------------------------
struct MotionDomain {
    QList<MotionBody>     bodies;
    QList<MotionSpring>   springs;
    QList<MotionPendulum> pendulums;
    QList<MotionRamp>     ramps;

    double gravity        = 980.0;   // px/s²  (9.8 m/s² assuming 1 px ≈ 1 cm)
    bool   hasBoundary    = true;
    double boundaryLeft   =    0.0;
    double boundaryRight  = 2400.0;
    double boundaryTop    =    0.0;
    double boundaryBottom = 1600.0;
};

// ---------------------------------------------------------------------------
// MotionSolver
// Stateful integrator; mutates MotionDomain::bodies each tick.
// Integration method: symplectic (semi-implicit) Euler — energy-conserving
// for spring–mass systems, stable at typical 16 ms time steps.
// Collision response: impulse method with coefficient-of-restitution.
// ---------------------------------------------------------------------------
class MotionSolver final : public IMotionSolver {
public:
    void step(MotionDomain& domain, double dt) override;

private:
    static void accumulateForces(MotionDomain& domain);
    static void integrate(MotionDomain& domain, double dt);
    static void resolveCollisions(MotionDomain& domain);
    static void enforceBoundary(MotionDomain& domain);
    static void stepPendulums(MotionDomain& domain, double dt);
    static void resolveRampCollisions(MotionDomain& domain);
    static void writeBack(MotionDomain& domain);
};
