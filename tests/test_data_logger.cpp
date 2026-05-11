// ---------------------------------------------------------------------------
// test_data_logger.cpp
// Tests for DataLogger: channel auto-discovery, series ring-buffer,
// clearData(), and multi-channel independence.
// ---------------------------------------------------------------------------
#include <QTest>
#include <QApplication>

#include "ui/graph/DataLogger.h"
#include "components/BaseComponent.h"

// ---------------------------------------------------------------------------
// Minimal stub component — sets typeId/displayName without a paint() override.
// ---------------------------------------------------------------------------
class StubComponent final : public BaseComponent {
    Q_OBJECT
public:
    explicit StubComponent(const QString& type, const QString& name,
                           QGraphicsItem* parent = nullptr)
        : BaseComponent(parent)
    {
        typeId      = type;
        displayName = name;
        id          = type + "_test";
    }

    QRectF boundingRect() const override { return QRectF(-1, -1, 2, 2); }
    void   paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override {}
};

// ---------------------------------------------------------------------------
class TestDataLogger : public QObject {
    Q_OBJECT

private slots:

    // ── A whitelisted component automatically creates a DataSeries ─────────
    void test_ammeter_creates_series()
    {
        DataLogger logger;
        StubComponent amm("ELEC_AMM", "Ammeter");
        amm.simState["current"] = 0.005;

        logger.setComponents({ &amm });
        logger.onTick(1.0);

        QCOMPARE(logger.series().size(), 1);
        QCOMPARE(logger.series()[0].channelId, QString("ELEC_AMM_test:current"));
        QVERIFY(qAbs(logger.series()[0].points.last().y() - 0.005) < 1e-9);
    }

    // ── A component with no matching key should produce no series ──────────
    void test_unknown_type_no_series()
    {
        DataLogger logger;
        StubComponent unknown("UNKNOWN_TYPE", "Gadget");
        unknown.simState["power"] = 42.0;

        logger.setComponents({ &unknown });
        logger.onTick(1.0);

        QCOMPARE(logger.series().size(), 0);
    }

    // ── Multiple ticks accumulate points ──────────────────────────────────
    void test_multiple_ticks_accumulate()
    {
        DataLogger logger;
        StubComponent ball("MOT_BALL", "Ball");

        logger.setComponents({ &ball });

        for (int i = 1; i <= 10; ++i) {
            ball.simState["speed"] = static_cast<double>(i * 10);
            logger.onTick(static_cast<double>(i) * 0.016);
        }

        QCOMPARE(logger.series().size(), 1);
        QCOMPARE(logger.series()[0].points.size(), 10);
        QVERIFY(qAbs(logger.series()[0].points.last().y() - 100.0) < 1e-9);
    }

    // ── Ring buffer caps at MAX_POINTS ────────────────────────────────────
    void test_ring_buffer_cap()
    {
        DataLogger logger;
        StubComponent det("WAV_DET", "Detector");

        logger.setComponents({ &det });

        const int OVER = DataSeries::MAX_POINTS + 100;
        for (int i = 0; i < OVER; ++i) {
            det.simState["amplitude"] = static_cast<double>(i);
            logger.onTick(static_cast<double>(i) * 0.016);
        }

        QCOMPARE(logger.series()[0].points.size(), DataSeries::MAX_POINTS);
        // Last recorded value should be the most recent.
        QVERIFY(qAbs(logger.series()[0].points.last().y() - (OVER - 1)) < 1e-9);
    }

    // ── clearData() removes all series and resets the time counter ─────────
    void test_clear_data()
    {
        DataLogger logger;
        StubComponent voltm("ELEC_VOLTM", "Voltmeter");
        voltm.simState["voltage"] = 3.3;

        logger.setComponents({ &voltm });
        logger.onTick(1.0);

        QCOMPARE(logger.series().size(), 1);
        QVERIFY(logger.currentTime() > 0.0);

        logger.clearData();

        QCOMPARE(logger.series().size(), 0);
        QVERIFY(logger.currentTime() == 0.0);

        // New data should create a fresh series after clearing.
        voltm.simState["voltage"] = 5.0;
        logger.onTick(2.0);
        QCOMPARE(logger.series().size(), 1);
        QVERIFY(qAbs(logger.series()[0].points.first().y() - 5.0) < 1e-9);
    }

    // ── Multiple component types each get their own series ─────────────────
    void test_multiple_components_independent_series()
    {
        DataLogger logger;
        StubComponent amm("ELEC_AMM",   "Ammeter");
        StubComponent det("WAV_DET",    "Detector");
        StubComponent ball("MOT_BALL",  "Ball");

        amm.simState["current"]   = 0.01;
        det.simState["amplitude"] = 1.5;
        ball.simState["speed"]    = 250.0;

        logger.setComponents({ &amm, &det, &ball });
        logger.onTick(0.5);

        QCOMPARE(logger.series().size(), 3);

        // Channel IDs should be distinct.
        QSet<QString> ids;
        for (const auto& s : logger.series())
            ids.insert(s.channelId);
        QCOMPARE(ids.size(), 3);
    }

    // ── setComponents() after data: existing series survive ───────────────
    void test_setComponents_preserves_existing_series()
    {
        DataLogger logger;
        StubComponent ball("MOT_BALL", "Ball");
        ball.simState["speed"] = 100.0;

        logger.setComponents({ &ball });
        logger.onTick(1.0);
        QCOMPARE(logger.series().size(), 1);

        // Rebuild components (simulates scene refresh).
        logger.setComponents({ &ball });
        ball.simState["speed"] = 200.0;
        logger.onTick(2.0);

        // Series should still exist and now have 2 points.
        QCOMPARE(logger.series().size(), 1);
        QCOMPARE(logger.series()[0].points.size(), 2);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestDataLogger test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_data_logger.moc"
