#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

class BaseComponent;

// ---------------------------------------------------------------------------
// DataSeries
// Ring-buffered time-series for one measurement channel.
//
//   channelId : "<componentId>:<simStateKey>"  (unique key)
//   label     : "<displayName> (<unit>)"        (shown in legend)
//   points    : QPointF(simTime, value), max MAX_POINTS entries (oldest dropped)
//   yMin/yMax : running extremes of all recorded values
// ---------------------------------------------------------------------------
struct DataSeries {
    QString        channelId;
    QString        label;
    QColor         color;
    QList<QPointF> points;      // QPointF.x = simTime (s), .y = value
    double         yMin = 0.0;
    double         yMax = 1.0;

    static constexpr int MAX_POINTS = 1800; // 30 s at 60 FPS
};

// ---------------------------------------------------------------------------
// DataLogger
// Attaches to SimulationLoop::tickComplete and records whitelisted simState
// values from scene components into DataSeries ring buffers.
//
// Whitelisted channels (typeId → simState key → unit label):
//   ELEC_AMM   → current   → A
//   ELEC_VOLTM → voltage   → V
//   MOT_BALL   → speed     → px/s
//   WAV_DET    → amplitude → amp
//
// Series are created automatically the first time a value is seen.
// Call setComponents() after every scene change; call clearData() on reset.
// ---------------------------------------------------------------------------
class DataLogger : public QObject {
    Q_OBJECT
public:
    explicit DataLogger(QObject* parent = nullptr);

    // Rebuild the tracking table from current scene contents.
    // Preserves existing series data; only adds/removes tracks.
    void setComponents(const QList<BaseComponent*>& components);

    const QList<DataSeries>& series() const { return m_series; }
    double currentTime() const { return m_lastTime; }

    void clearData();

public slots:
    void onTick(double simTime);

signals:
    void seriesUpdated();

private:
    struct Track {
        BaseComponent* comp     = nullptr;
        QString        key;         // simState key to watch
        QString        channelId;   // "<componentId>:<key>"
        QString        label;       // legend text
        int            seriesIdx = -1; // index into m_series; -1 = not yet created
    };

    QList<Track>      m_tracks;
    QList<DataSeries> m_series;
    double            m_lastTime = 0.0;

    static QColor colorForIndex(int idx);
};
