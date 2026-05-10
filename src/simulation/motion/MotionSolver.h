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
// MotionDomain
// Complete mechanical scene snapshot.  Persists across ticks so velocity
// state is maintained.  Rebuilt from scene on structural changes (not on
// every tick).
// ---------------------------------------------------------------------------
struct MotionDomain {
    QList<MotionBody>   bodies;
    QList<MotionSpring> springs;

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
    static void writeBack(MotionDomain& domain);
};
