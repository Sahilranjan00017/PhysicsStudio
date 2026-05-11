#include "ui/graph/DataLogger.h"

#include "components/BaseComponent.h"

#include <QtMath>

// ---------------------------------------------------------------------------
// Whitelist: which (typeId, simStateKey, unitSuffix) triples are logged.
// ---------------------------------------------------------------------------
namespace {

struct ChannelSpec { const char* typeId; const char* key; const char* unit; };

constexpr ChannelSpec CHANNELS[] = {
    { "ELEC_AMM",   "current",      "A"    },
    { "ELEC_VOLTM", "voltageDiff",  "V"    },  // fix: was "voltage"
    { "ELEC_SCOPE", "voltageDiff",  "V"    },
    { "ELEC_IND",   "i_ind",        "A"    },
    { "MOT_BALL",   "speed",        "px/s" },
    { "MOT_PENDULUM","angle",       "rad"  },
    { "WAV_DET",    "amplitude",    "amp"  },
};
constexpr int NUM_CHANNELS = static_cast<int>(sizeof(CHANNELS) / sizeof(CHANNELS[0]));

} // namespace

// ---------------------------------------------------------------------------
DataLogger::DataLogger(QObject* parent) : QObject(parent) {}

// ---------------------------------------------------------------------------
QColor DataLogger::colorForIndex(int idx)
{
    static const QColor PALETTE[] = {
        QColor(86,  156, 214),   // VS Code blue
        QColor(244, 135,  52),   // orange
        QColor(106, 198, 118),   // green
        QColor(220,  90,  90),   // red
        QColor(180, 130, 240),   // purple
        QColor(240, 220,  80),   // yellow
    };
    constexpr int N = static_cast<int>(sizeof(PALETTE) / sizeof(PALETTE[0]));
    return PALETTE[idx % N];
}

// ---------------------------------------------------------------------------
void DataLogger::setComponents(const QList<BaseComponent*>& components)
{
    // Rebuild the track list.  Preserve existing series so clearing the graph
    // only happens explicitly via clearData().
    m_tracks.clear();

    for (auto* comp : components) {
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            if (comp->typeId != QLatin1String(CHANNELS[i].typeId))
                continue;

            Track t;
            t.comp      = comp;
            t.key       = QLatin1String(CHANNELS[i].key);
            t.channelId = comp->id + ':' + t.key;
            t.label     = comp->displayName + " (" + QLatin1String(CHANNELS[i].unit) + ')';
            t.seriesIdx = -1;

            // Reconnect to an existing series if one already exists for this channel.
            for (int si = 0; si < m_series.size(); ++si) {
                if (m_series[si].channelId == t.channelId) {
                    t.seriesIdx = si;
                    break;
                }
            }

            m_tracks.append(t);
        }
    }
}

// ---------------------------------------------------------------------------
void DataLogger::clearData()
{
    m_series.clear();
    for (auto& t : m_tracks)
        t.seriesIdx = -1;
    m_lastTime = 0.0;
}

// ---------------------------------------------------------------------------
void DataLogger::onTick(double simTime)
{
    m_lastTime = simTime;
    bool any   = false;

    for (auto& track : m_tracks) {
        if (!track.comp)
            continue;
        if (!track.comp->simState.contains(track.key))
            continue;

        bool ok = false;
        const double val = track.comp->simState.value(track.key).toDouble(&ok);
        if (!ok)
            continue;

        // Auto-create a DataSeries on the first recorded value.
        if (track.seriesIdx < 0) {
            DataSeries ds;
            ds.channelId = track.channelId;
            ds.label     = track.label;
            ds.color     = colorForIndex(m_series.size());
            ds.yMin      = val;
            ds.yMax      = val;
            track.seriesIdx = m_series.size();
            m_series.append(ds);
        }

        DataSeries& ds = m_series[track.seriesIdx];

        // Enforce ring-buffer cap.
        if (ds.points.size() >= DataSeries::MAX_POINTS)
            ds.points.removeFirst();

        ds.points.append(QPointF(simTime, val));
        ds.yMin = qMin(ds.yMin, val);
        ds.yMax = qMax(ds.yMax, val);
        any = true;
    }

    if (any)
        emit seriesUpdated();
}
