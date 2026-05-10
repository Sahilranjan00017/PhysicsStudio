#include "components/wave/WaveComponents.h"

#include "components/ComponentRegistry.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionGraphicsItem>

#include <cmath>

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------
namespace {

QPen wavePen(bool selected, QColor base = QColor(35, 42, 50))
{
    return QPen(selected ? QColor(28, 100, 242) : base,
                selected ? 2.5 : 2.0);
}

ComponentDescriptor waveDescriptor(
    const QString& typeId,
    const QString& displayName,
    const QString& description,
    const QMap<QString, QVariant>& props = {})
{
    return ComponentDescriptor{typeId, displayName, "Waves", description, props};
}

} // namespace

// ===========================================================================
// WaveSourceComponent
// ===========================================================================

WaveSourceComponent::WaveSourceComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_SRC";
    displayName = "Wave Source";
}

QRectF WaveSourceComponent::boundingRect() const
{
    return QRectF(-44.0, -44.0, 88.0, 100.0);
}

void WaveSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   selected  = option->state & QStyle::State_Selected;
    const double freq      = properties.value("frequency", 2.0).toDouble();
    const double t         = simState.value("simTime",     0.0).toDouble();

    // Animated outgoing ring phase (0–1, cycling at frequency).
    const double ringPhase = std::fmod(t * freq, 1.0);

    // Draw three fading rings that emanate outward.
    for (int i = 0; i < 3; ++i) {
        const double phase = std::fmod(ringPhase + i / 3.0, 1.0);
        const double r     = phase * 38.0 + 6.0;         // 6 → 44 px
        const double alpha = (1.0 - phase) * 0.6;        // fade out

        painter->setPen(QPen(QColor(40, 130, 220, static_cast<int>(alpha * 255)), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), r, r);
    }

    // Central source body.
    painter->setPen(wavePen(selected));
    painter->setBrush(QColor(200, 230, 255));
    painter->drawEllipse(QPointF(0.0, 0.0), 12.0, 12.0);

    // "~" wave symbol inside.
    painter->setPen(QPen(QColor(20, 80, 160), 1.5));
    const double wx = 6.0, wy = 4.0;
    painter->drawArc(QRectF(-wx, -wy, wx, 2 * wy), 0, 180 * 16);
    painter->drawArc(QRectF(0,  -wy, wx, 2 * wy), 180 * 16, 180 * 16);

    // Frequency label below.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-40.0, 16.0, 80.0, 14.0), Qt::AlignCenter,
                      QString("%1 Hz").arg(freq, 0, 'f', 1));
}

// ===========================================================================
// WaveDetectorComponent
// ===========================================================================

WaveDetectorComponent::WaveDetectorComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_DET";
    displayName = "Wave Detector";
}

QRectF WaveDetectorComponent::boundingRect() const
{
    return QRectF(-38.0, -28.0, 76.0, 62.0);
}

void WaveDetectorComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   selected  = option->state & QStyle::State_Selected;
    const bool   hasReading = simState.contains("amplitude");
    const double amp       = simState.value("amplitude", 0.0).toDouble();

    // Body colour shifts positive (warm) ↔ negative (cool) with amplitude.
    const int absAmp = static_cast<int>(std::min(std::abs(amp) * 60.0, 80.0));
    const QColor fill = hasReading
        ? (amp >= 0 ? QColor(255, 200 - absAmp, 150 - absAmp)
                    : QColor(150 - absAmp, 200 - absAmp, 255))
        : QColor(230, 230, 230);

    painter->setPen(wavePen(selected));
    painter->setBrush(fill);
    painter->drawEllipse(QPointF(0.0, 0.0), 22.0, 22.0);

    // Amplitude reading.
    painter->setPen(QColor(35, 42, 50));
    const QString label = hasReading
        ? QString("%1").arg(amp, 0, 'f', 2)
        : "~";
    painter->drawText(QRectF(-20.0, -10.0, 40.0, 20.0), Qt::AlignCenter, label);

    // "Detector" label below.
    painter->drawText(QRectF(-36.0, 26.0, 72.0, 14.0), Qt::AlignCenter, "Detector");
}

// ===========================================================================
// Registration
// ===========================================================================

void registerWaveComponents(ComponentRegistry& registry)
{
    registry.registerType(
        waveDescriptor("WAV_SRC", "Wave Source",
                       "Emits circular harmonic waves — place 2+ sources to see interference",
                       {
                           { "frequency",  2.0 },   // Hz
                           { "amplitude",  1.0 },
                           { "phase",      0.0 },   // degrees
                       }),
        [] { return new WaveSourceComponent(); });

    registry.registerType(
        waveDescriptor("WAV_DET", "Wave Detector",
                       "Measures superposed wave amplitude at this point each tick"),
        [] { return new WaveDetectorComponent(); });
}
