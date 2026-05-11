// ---------------------------------------------------------------------------
// test_wave_solver.cpp
// Tests the WaveSolver analytical superposition engine.
//
// WaveSolver reads only BaseComponent::properties and BaseComponent::pos(),
// so we use minimal inline stub subclasses instead of the real
// WaveSourceComponent/WaveDetectorComponent (which would drag in the full
// ComponentRegistry).
// ---------------------------------------------------------------------------
#include <QTest>
#include <QApplication>
#include <QPainter>           // required by BaseComponent::paint signature
#include <QStyleOptionGraphicsItem>
#include <QtMath>

#include "simulation/wave/WaveSolver.h"
#include "components/BaseComponent.h"

// ---------------------------------------------------------------------------
// Minimal stubs — WaveSolver accesses only properties, pos(), and simState.
// ---------------------------------------------------------------------------
class StubSource final : public BaseComponent {
    Q_OBJECT
public:
    explicit StubSource(double x, double y,
                        double freq = 1.0, double amp = 1.0, double phase = 0.0)
        : BaseComponent()
    {
        typeId      = "WAV_SRC";
        displayName = "Source";
        id          = "src_test";
        properties["frequency"] = freq;
        properties["amplitude"] = amp;
        properties["phase"]     = phase;
        setPos(x, y);
    }
    QRectF boundingRect() const override { return QRectF(-1, -1, 2, 2); }
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}
};

class StubDetector final : public BaseComponent {
    Q_OBJECT
public:
    explicit StubDetector(double x, double y) : BaseComponent()
    {
        typeId      = "WAV_DET";
        displayName = "Detector";
        id          = "det_test";
        setPos(x, y);
    }
    QRectF boundingRect() const override { return QRectF(-1, -1, 2, 2); }
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}
};

// ---------------------------------------------------------------------------
class TestWaveSolver : public QObject {
    Q_OBJECT

private:
    static WaveDomain makeDomain(int cols = 20, int rows = 20,
                                 double gridSize = 10.0)
    {
        WaveDomain d;
        d.cols      = cols;
        d.rows      = rows;
        d.gridSize  = gridSize;
        d.waveSpeed = 200.0;
        d.simTime   = 0.0;
        return d;
    }

private slots:

    // ── Single source produces a non-zero field ───────────────────────────
    void test_single_source_nonzero_field()
    {
        WaveSolver solver;
        WaveDomain domain = makeDomain();

        StubSource src(0.0, 0.0, 1.0, 1.0, 0.0);
        domain.sources << &src;

        solver.step(domain, 0.25); // quarter period → sin at peak

        bool anyNonZero = false;
        for (size_t i = 1; i < domain.field.size(); ++i) {
            if (domain.field[i] != 0.0f) { anyNonZero = true; break; }
        }
        QVERIFY(anyNonZero);
    }

    // ── No sources → field never allocated ───────────────────────────────
    void test_no_sources_field_empty()
    {
        WaveSolver solver;
        WaveDomain domain = makeDomain();
        // No sources.
        solver.step(domain, 0.1);
        QVERIFY(domain.field.empty());
    }

    // ── Superposition linearity: two co-located sources = 2× amplitude ───
    void test_superposition_linearity()
    {
        WaveSolver solver;

        WaveDomain d1 = makeDomain(10, 10);
        StubSource s1(50.0, 50.0, 2.0, 1.0, 0.0);
        d1.sources << &s1;
        solver.step(d1, 0.1);

        WaveDomain d2 = makeDomain(10, 10);
        StubSource sA(50.0, 50.0, 2.0, 1.0, 0.0);
        StubSource sB(50.0, 50.0, 2.0, 1.0, 0.0);
        d2.sources << &sA << &sB;
        solver.step(d2, 0.1);

        QCOMPARE(d1.field.size(), d2.field.size());

        for (size_t i = 0; i < d1.field.size(); ++i) {
            const float expected = d1.field[i] * 2.0f;
            const float actual   = d2.field[i];
            QVERIFY(qAbs(static_cast<double>(actual - expected)) < 1e-4);
        }
    }

    // ── Detector reads the field value at its grid cell ───────────────────
    void test_detector_reads_field_cell()
    {
        WaveSolver solver;
        WaveDomain domain = makeDomain(30, 30, 10.0);

        StubSource src(0.0, 0.0, 2.0, 1.0, 0.0);
        domain.sources << &src;

        StubDetector det(50.0, 50.0); // → grid cell (5, 5)
        domain.detectors << &det;

        solver.step(domain, 0.1);

        QVERIFY(det.simState.contains("amplitude"));

        const float fieldVal = domain.field[static_cast<size_t>(5 * 30 + 5)];
        const double detVal  = det.simState["amplitude"].toDouble();
        QVERIFY(qAbs(detVal - static_cast<double>(fieldVal)) < 1e-5);
    }

    // ── Source cell itself is zero (singularity guard r < 0.5) ───────────
    void test_source_cell_is_zero()
    {
        WaveSolver solver;
        WaveDomain domain = makeDomain(10, 10, 10.0);

        StubSource src(0.0, 0.0); // lands on cell (0, 0)
        domain.sources << &src;
        solver.step(domain, 0.25);

        QVERIFY(domain.field[0] == 0.0f);
    }

    // ── simTime accumulates across ticks ─────────────────────────────────
    void test_simtime_advances()
    {
        WaveSolver solver;
        WaveDomain domain = makeDomain(5, 5);

        StubSource src(25.0, 25.0, 1.0, 1.0, 0.0);
        domain.sources << &src;

        const double dt = 0.016;
        for (int i = 0; i < 10; ++i)
            solver.step(domain, dt);

        QVERIFY(qAbs(domain.simTime - 10.0 * dt) < 1e-9);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestWaveSolver test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_wave_solver.moc"
