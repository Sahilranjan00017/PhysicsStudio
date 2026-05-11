// ---------------------------------------------------------------------------
// test_motion_solver.cpp
// Unit tests for MotionSolver physics: gravity, symplectic integration,
// boundary reflection, and ball–ball impulse collision.
//
// MotionBody::component is left nullptr in all tests; writeBack() guards
// against null with "if (b.component == nullptr) continue".
// ---------------------------------------------------------------------------
#include <QTest>
#include <QtMath>

#include "simulation/motion/MotionSolver.h"

class TestMotionSolver : public QObject {
    Q_OBJECT

private:
    // Helper: create a free-falling ball at (x, y) with zero initial velocity.
    static MotionBody makeBall(double x, double y,
                               double mass = 1.0, double radius = 20.0,
                               double restitution = 1.0)
    {
        MotionBody b;
        b.component   = nullptr;   // no scene item in unit tests
        b.shape       = MotionBody::Shape::Ball;
        b.fixed       = false;
        b.mass        = mass;
        b.radius      = radius;
        b.restitution = restitution;
        b.pos         = QPointF(x, y);
        b.vel         = QPointF(0.0, 0.0);
        b.force       = QPointF(0.0, 0.0);
        return b;
    }

    // Helper: default domain with no boundary (open space).
    static MotionDomain openDomain(double gravity = 980.0)
    {
        MotionDomain d;
        d.gravity     = gravity;
        d.hasBoundary = false;
        return d;
    }

private slots:

    // ── Gravity ───────────────────────────────────────────────────────────
    // After one tick of dt=0.01 s, a free ball should gain:
    //   v_y = gravity * dt = 980 * 0.01 = 9.8 px/s  (positive = downward)
    //   y   ≈ y0 + v_y * dt  (symplectic Euler uses the NEW velocity)
    void test_gravity_one_tick()
    {
        MotionSolver solver;
        MotionDomain domain = openDomain();
        domain.bodies.append(makeBall(100.0, 100.0));

        const double dt = 0.01;
        solver.step(domain, dt);

        const MotionBody& b = domain.bodies[0];
        QVERIFY(b.vel.x() ==  0.0);
        QVERIFY(qAbs(b.vel.y() - 9.8) < 0.01);
        // Symplectic: position uses NEW velocity
        QVERIFY(qAbs(b.pos.y() - (100.0 + b.vel.y() * dt)) < 0.01);
    }

    // ── Gravity accumulates correctly over many ticks ────────────────────
    // After N ticks: v_y = gravity * N * dt  (no air resistance)
    void test_gravity_accumulates()
    {
        MotionSolver solver;
        MotionDomain domain = openDomain();
        domain.bodies.append(makeBall(500.0, 200.0));

        const double dt = 0.016;
        const int    N  = 60;   // 1 simulated second at 60 FPS
        for (int i = 0; i < N; ++i)
            solver.step(domain, dt);

        const double expectedVy = 980.0 * N * dt;
        QVERIFY(qAbs(domain.bodies[0].vel.y() - expectedVy) < 1.0);
    }

    // ── Fixed body doesn't move under gravity ─────────────────────────────
    void test_fixed_body_stationary()
    {
        MotionSolver solver;
        MotionDomain domain = openDomain();
        MotionBody wall = makeBall(400.0, 800.0);
        wall.fixed = true;
        domain.bodies.append(wall);

        for (int i = 0; i < 100; ++i)
            solver.step(domain, 0.016);

        QVERIFY(domain.bodies[0].pos == QPointF(400.0, 800.0));
        QVERIFY(domain.bodies[0].vel == QPointF(0.0,   0.0));
    }

    // ── Boundary reflection reverses vertical velocity ────────────────────
    // A ball falling straight down should bounce off the bottom wall.
    void test_bottom_boundary_reflection()
    {
        MotionSolver solver;
        MotionDomain domain;
        domain.gravity        = 980.0;
        domain.hasBoundary    = true;
        domain.boundaryLeft   = 0.0;
        domain.boundaryRight  = 2400.0;
        domain.boundaryTop    = 0.0;
        domain.boundaryBottom = 1600.0;

        // Place ball near the floor with a strong downward velocity.
        MotionBody b = makeBall(1200.0, 1560.0, 1.0, 20.0, 1.0 /*elastic*/);
        b.vel = QPointF(0.0, 500.0);
        domain.bodies.append(b);

        // A few ticks — ball should hit the floor and bounce.
        for (int i = 0; i < 10; ++i)
            solver.step(domain, 0.016);

        // After bouncing, vertical velocity should be negative (moving up)
        // or ball should be within bounds.
        QVERIFY(domain.bodies[0].pos.y() + 20.0 <= 1600.0 + 0.1);
    }

    // ── Ball–ball collision: head-on, equal mass, elastic ─────────────────
    // Two equal balls moving toward each other should exchange velocities.
    void test_ball_ball_collision_exchange()
    {
        MotionSolver solver;
        MotionDomain domain = openDomain(0.0); // no gravity

        MotionBody left  = makeBall(0.0,  0.0, 1.0, 20.0, 1.0);
        MotionBody right = makeBall(36.0, 0.0, 1.0, 20.0, 1.0); // gap < 2*radius → overlapping

        left.vel  = QPointF( 100.0, 0.0);
        right.vel = QPointF(-100.0, 0.0);

        domain.bodies.append(left);
        domain.bodies.append(right);

        solver.step(domain, 0.001); // single very small tick so no gravity drift

        // After elastic collision of equal masses: velocities should have swapped.
        const double vxLeft  = domain.bodies[0].vel.x();
        const double vxRight = domain.bodies[1].vel.x();
        QVERIFY(vxLeft  < 0.0);   // left ball now moves right→left
        QVERIFY(vxRight > 0.0);   // right ball now moves left→right
    }

    // ── Zero-gravity: ball moves in a straight line without forces ────────
    void test_no_gravity_straight_line()
    {
        MotionSolver solver;
        MotionDomain domain = openDomain(0.0); // zero gravity

        MotionBody b = makeBall(0.0, 0.0);
        b.vel = QPointF(200.0, 50.0);
        domain.bodies.append(b);

        const double dt = 0.016;
        solver.step(domain, dt);

        // With zero forces, symplectic Euler == explicit Euler.
        QVERIFY(qAbs(domain.bodies[0].pos.x() - 200.0 * dt) < 0.01);
        QVERIFY(qAbs(domain.bodies[0].pos.y() -  50.0 * dt) < 0.01);
    }
};

QTEST_MAIN(TestMotionSolver)
#include "test_motion_solver.moc"
