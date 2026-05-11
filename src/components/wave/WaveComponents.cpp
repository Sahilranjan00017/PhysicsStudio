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
// WaveBarrierComponent
// ===========================================================================

WaveBarrierComponent::WaveBarrierComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_BARRIER";
    displayName = "Barrier";
}

QRectF WaveBarrierComponent::boundingRect() const
{
    const double halfL = properties.value("barrierLength", 200.0).toDouble() * 0.5 + 6.0;
    return QRectF(-24.0, -halfL, 60.0, 2.0 * halfL + 20.0);
}

void WaveBarrierComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfL   = properties.value("barrierLength",  200.0).toDouble() * 0.5;
    const double slitW   = properties.value("slitWidth",       16.0).toDouble();
    const double slitSep = properties.value("slitSeparation",  60.0).toDouble();
    const int    numSlits= std::clamp(properties.value("numSlits", 1).toInt(), 1, 2);
    const bool   selected = option->state & QStyle::State_Selected;

    const QPen barrierPen(selected ? QColor(28, 100, 242) : QColor(50, 55, 65),
                          selected ? 2.5 : 2.0);

    QList<double> gapCentres;
    if (numSlits == 1) {
        gapCentres << 0.0;
    } else {
        gapCentres << -slitSep * 0.5 << slitSep * 0.5;
    }

    // Compute opaque segments.
    QList<QPair<double,double>> opaque;
    double prev = -halfL;
    for (double gc : gapCentres) {
        double gTop = gc - slitW * 0.5;
        double gBot = gc + slitW * 0.5;
        if (prev < gTop) opaque.append({prev, gTop});
        prev = gBot;
    }
    if (prev < halfL) opaque.append({prev, halfL});

    painter->setPen(barrierPen);
    painter->setBrush(QColor(50, 55, 65));
    for (const auto& seg : opaque) {
        const double segH = seg.second - seg.first;
        painter->drawRect(QRectF(-6.0, seg.first, 12.0, segH));
    }

    // Slit gap indicator lines.
    painter->setPen(QPen(QColor(28, 100, 242, 120), 1.0, Qt::DashLine));
    for (double gc : gapCentres) {
        painter->drawLine(QPointF(-6.0, gc), QPointF(6.0, gc));
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-20.0, halfL + 4.0, 60.0, 14.0), Qt::AlignLeft,
                      numSlits == 2 ? "2-Slit" : "1-Slit");
}

// ===========================================================================
// WaveSoundComponent
// ===========================================================================

WaveSoundComponent::WaveSoundComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_SOUND";
    displayName = "Speaker";
}

QRectF WaveSoundComponent::boundingRect() const
{
    return QRectF(-48.0, -36.0, 96.0, 88.0);
}

void WaveSoundComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   selected = option->state & QStyle::State_Selected;
    const double freq     = properties.value("frequency", 440.0).toDouble();
    const double t        = simState.value("simTime", 0.0).toDouble();

    // Speaker cone body (trapezoid).
    painter->setPen(wavePen(selected));
    painter->setBrush(QColor(60, 65, 75));
    const QPolygonF cone {
        QPointF(-10.0, -16.0), QPointF(-10.0, 16.0),
        QPointF( 18.0,  28.0), QPointF( 18.0, -28.0),
    };
    painter->drawPolygon(cone);

    // Speaker cabinet (rect).
    painter->setBrush(QColor(80, 85, 90));
    painter->drawRect(QRectF(-26.0, -28.0, 16.0, 56.0));

    // Sound waves radiating to the right.
    const double phase = std::fmod(t * freq * 0.5, 1.0);
    painter->setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        const double p = std::fmod(phase + i / 3.0, 1.0);
        const double r = p * 28.0 + 8.0;
        const double alpha = (1.0 - p) * 0.7;
        painter->setPen(QPen(QColor(40, 130, 220, static_cast<int>(alpha * 255)), 1.5));
        painter->drawArc(QRectF(18.0 - r, -r, 2.0 * r, 2.0 * r), -70 * 16, 140 * 16);
    }

    // Frequency label.
    painter->setPen(QColor(35, 42, 50));
    painter->drawText(QRectF(-40.0, 30.0, 80.0, 14.0), Qt::AlignCenter,
                      QString("%1 Hz").arg(freq, 0, 'f', 0));
}

// ===========================================================================
// WaveWallComponent
// ===========================================================================

WaveWallComponent::WaveWallComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_WALL";
    displayName = "Wave Wall";
}

QRectF WaveWallComponent::boundingRect() const
{
    const double halfL = properties.value("length", 160.0).toDouble() * 0.5 + 6.0;
    return QRectF(-20.0, -halfL, 56.0, 2.0 * halfL + 20.0);
}

void WaveWallComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfL      = properties.value("length",     160.0).toDouble() * 0.5;
    const double reflectance= properties.value("reflectance",  0.9).toDouble();
    const bool   sel        = option->state & QStyle::State_Selected;

    // Wall body — solid dark slab.
    painter->setPen(wavePen(sel, QColor(50, 55, 65)));
    painter->setBrush(QColor(55, 60, 70));
    painter->drawRect(QRectF(-8.0, -halfL, 16.0, 2.0 * halfL));

    // Reflective face — bright line on the +X side.
    painter->setPen(QPen(sel ? QColor(28, 100, 242) : QColor(190, 210, 240), 3.0));
    painter->drawLine(QPointF(8.0, -halfL), QPointF(8.0, halfL));

    // Reflection hatch marks on the +X (reflecting) side.
    painter->setPen(QPen(QColor(150, 165, 185), 1.0));
    const int hN = static_cast<int>(2.0 * halfL / 12.0);
    for (int i = 0; i <= hN; ++i) {
        const double y = -halfL + i * (2.0 * halfL) / std::max(hN, 1);
        painter->drawLine(QPointF(8.0, y), QPointF(18.0, y - 8.0));
    }

    // Reflectance label.
    painter->setPen(QColor(35, 42, 50));
    painter->setFont(QFont("Arial", 7));
    painter->drawText(QRectF(-18.0, halfL + 4.0, 60.0, 14.0), Qt::AlignLeft,
                      QString("R=%1%").arg(static_cast<int>(reflectance * 100)));
}

// ===========================================================================
// WavePlaneSourceComponent
// ===========================================================================

WavePlaneSourceComponent::WavePlaneSourceComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_PLANE";
    displayName = "Plane Wave";
}

QRectF WavePlaneSourceComponent::boundingRect() const
{
    const double halfW = properties.value("width", 120.0).toDouble() * 0.5 + 6.0;
    return QRectF(-14.0, -halfW, 72.0, 2.0 * halfW + 20.0);
}

void WavePlaneSourceComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double halfW = properties.value("width",     120.0).toDouble() * 0.5;
    const double freq  = properties.value("frequency",   2.0).toDouble();
    const double t     = simState.value("simTime",       0.0).toDouble();
    const bool   sel   = option->state & QStyle::State_Selected;

    // Source bar (vertical) — the emission line.
    painter->setPen(wavePen(sel, QColor(30, 120, 200)));
    painter->setBrush(QColor(180, 215, 250));
    painter->drawRect(QRectF(-6.0, -halfW, 12.0, 2.0 * halfW));

    // Tick marks along the bar.
    painter->setPen(QPen(QColor(20, 80, 160), 1.2));
    const int numTicks = std::clamp(static_cast<int>(2.0 * halfW / 12.0), 2, 16);
    for (int i = 0; i <= numTicks; ++i) {
        const double y = -halfW + i * (2.0 * halfW) / numTicks;
        painter->drawLine(QPointF(6.0, y), QPointF(14.0, y));
    }

    // Animated planar wavefronts (short vertical lines scrolling to the right).
    const double phase   = std::fmod(t * freq, 1.0);
    const double lambda  = 60.0;  // visual wavelength in px
    painter->setBrush(Qt::NoBrush);
    for (int i = 0; i < 3; ++i) {
        const double x     = 14.0 + (std::fmod(phase + i / 3.0, 1.0)) * lambda;
        const double alpha = 1.0 - std::fmod(phase + i / 3.0, 1.0);
        painter->setPen(QPen(QColor(40, 130, 220, static_cast<int>(alpha * 200)), 1.5));
        painter->drawLine(QPointF(x, -halfW * 0.85), QPointF(x, halfW * 0.85));
    }

    // Main direction arrow.
    painter->setPen(QPen(QColor(20, 80, 160), 2.0));
    painter->drawLine(QPointF(14.0, 0.0), QPointF(46.0, 0.0));
    painter->setBrush(QColor(20, 80, 160));
    painter->setPen(Qt::NoPen);
    painter->drawPolygon(QPolygonF({
        QPointF(46.0,  0.0),
        QPointF(38.0, -5.0),
        QPointF(38.0,  5.0),
    }));

    // Frequency label.
    painter->setPen(QColor(35, 42, 50));
    painter->setFont(QFont("Arial", 7));
    painter->drawText(QRectF(-12.0, halfW + 4.0, 70.0, 14.0), Qt::AlignCenter,
                      QString("%1 Hz").arg(freq, 0, 'f', 1));
}

// ===========================================================================
// WaveAbsorberComponent
// ===========================================================================

WaveAbsorberComponent::WaveAbsorberComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_ABSORBER";
    displayName = "Absorber";
}

QRectF WaveAbsorberComponent::boundingRect() const
{
    const double r = properties.value("radius", 60.0).toDouble();
    return QRectF(-(r + 4.0), -(r + 4.0), 2.0 * (r + 4.0), 2.0 * (r + 4.0) + 20.0);
}

void WaveAbsorberComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const double r    = properties.value("radius",  60.0).toDouble();
    const double damp = properties.value("damping",  0.95).toDouble();
    const bool   sel  = option->state & QStyle::State_Selected;

    // Absorber body — dark matte circle.
    painter->setPen(wavePen(sel, QColor(40, 44, 52)));
    painter->setBrush(QColor(45, 48, 55));
    painter->drawEllipse(QPointF(0.0, 0.0), r, r);

    // Cross-hatch fill indicating absorbing material.
    painter->setClipPath([&]{
        QPainterPath p; p.addEllipse(QPointF(0.0, 0.0), r - 1.0, r - 1.0); return p;
    }());
    painter->setPen(QPen(QColor(65, 70, 80), 1.0));
    const double step = 14.0;
    for (double d = -2.0 * r; d <= 2.0 * r; d += step) {
        painter->drawLine(QPointF(d - r, -r), QPointF(d + r, r));
        painter->drawLine(QPointF(d - r,  r), QPointF(d + r, -r));
    }
    painter->setClipping(false);

    // Outer ring.
    painter->setPen(wavePen(sel, QColor(40, 44, 52)));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(0.0, 0.0), r, r);

    // Damping label.
    painter->setPen(QColor(200, 205, 215));
    painter->setFont(QFont("Arial", 7, QFont::Bold));
    painter->drawText(QRectF(-r, -9.0, 2.0 * r, 18.0), Qt::AlignCenter,
                      QString("%1%").arg(static_cast<int>(damp * 100)));
    painter->setPen(QColor(35, 42, 50));
    painter->setFont(QFont("Arial", 7));
    painter->drawText(QRectF(-r, r + 4.0, 2.0 * r, 14.0), Qt::AlignCenter, displayName);
}

// ===========================================================================
// WaveRippleComponent
// ===========================================================================

WaveRippleComponent::WaveRippleComponent(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId      = "WAV_RIPPLE";
    displayName = "Ripple";
}

QRectF WaveRippleComponent::boundingRect() const
{
    return QRectF(-48.0, -48.0, 96.0, 112.0);
}

void WaveRippleComponent::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool   sel       = option->state & QStyle::State_Selected;
    const double freq      = properties.value("frequency",  2.0).toDouble();
    const double numCycles = properties.value("numCycles",  3.0).toDouble();
    const double t         = simState.value("simTime",      0.0).toDouble();
    const double duration  = (freq > 0.0) ? (numCycles / freq) : 1.0;

    // Pulse envelope (0 when done).
    const double envelope = (t < duration && duration > 0.0)
                            ? std::sin(M_PI * t / duration)
                            : 0.0;

    // Central source dot.
    painter->setPen(wavePen(sel));
    painter->setBrush(envelope > 0.01 ? QColor(100, 180, 255) : QColor(180, 190, 200));
    painter->drawEllipse(QPointF(0.0, 0.0), 10.0, 10.0);

    // "Stone drop" icon — teardrop above the centre.
    painter->setBrush(QColor(80, 120, 180));
    painter->setPen(Qt::NoPen);
    painter->drawEllipse(QPointF(0.0, -18.0), 5.0, 5.0);
    painter->drawLine(QPointF(0.0, -13.0), QPointF(0.0, -10.0));

    // Animated expanding rings (only while pulse is active).
    const double ringPhase = std::fmod(t * freq, 1.0);
    for (int i = 0; i < static_cast<int>(numCycles) + 1; ++i) {
        const double ph = std::fmod(ringPhase + i / std::max(numCycles, 1.0), 1.0);
        const double r  = ph * 40.0 + 10.0;
        const double alpha = (1.0 - ph) * envelope * 0.7;
        if (alpha < 0.01) continue;
        painter->setPen(QPen(QColor(60, 140, 230, static_cast<int>(alpha * 255)), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QPointF(0.0, 0.0), r, r);
    }

    // Done indicator.
    if (envelope < 0.01 && t > 0.1) {
        painter->setPen(QColor(150, 80, 80));
        painter->setFont(QFont("Arial", 7));
        painter->drawText(QRectF(-22.0, 14.0, 44.0, 14.0), Qt::AlignCenter, "done");
    }

    // Label.
    painter->setPen(QColor(35, 42, 50));
    painter->setFont(QFont("Arial", 7));
    painter->drawText(QRectF(-40.0, 52.0, 80.0, 14.0), Qt::AlignCenter,
                      QString("%1 cy @ %2 Hz").arg(static_cast<int>(numCycles)).arg(freq, 0, 'f', 1));
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

    registry.registerType(
        waveDescriptor("WAV_BARRIER", "Barrier",
                       "Opaque barrier with 1 or 2 slits — slit openings act as Huygens sources",
                       {
                           { "barrierLength",  200.0 },
                           { "numSlits",         1   },
                           { "slitWidth",       16.0 },
                           { "slitSeparation",  60.0 },
                           { "frequency",        2.0 },  // Hz of secondary sources
                           { "amplitude",        0.6 },  // relative amplitude
                       }),
        [] { return new WaveBarrierComponent(); });

    registry.registerType(
        waveDescriptor("WAV_SOUND", "Speaker",
                       "Sound wave source — animated speaker icon, same physics as Wave Source",
                       {
                           { "frequency", 440.0 },   // Hz (A4 pitch default)
                           { "amplitude",   1.0 },
                           { "phase",       0.0 },
                       }),
        [] { return new WaveSoundComponent(); });

    registry.registerType(
        waveDescriptor("WAV_WALL", "Wave Wall",
                       "Reflective hard boundary — image-source method creates standing waves",
                       {
                           { "length",      160.0 },
                           { "reflectance",   0.9 },
                       }),
        [] { return new WaveWallComponent(); });

    registry.registerType(
        waveDescriptor("WAV_PLANE", "Plane Wave",
                       "Emits planar wavefronts from a line of coherent sources — rotate to aim",
                       {
                           { "frequency", 2.0   },
                           { "amplitude", 1.0   },
                           { "phase",     0.0   },
                           { "width",   120.0   },  // px — length of the source line
                       }),
        [] { return new WavePlaneSourceComponent(); });

    registry.registerType(
        waveDescriptor("WAV_ABSORBER", "Absorber",
                       "Anechoic absorbing region — damps wave field within its radius",
                       {
                           { "radius",  60.0 },
                           { "damping",  0.95 },  // fraction absorbed per tick
                       }),
        [] { return new WaveAbsorberComponent(); });

    registry.registerType(
        waveDescriptor("WAV_RIPPLE", "Ripple",
                       "Finite pulse source — like a stone in a ripple tank; restarts each simulation",
                       {
                           { "frequency",  2.0 },
                           { "amplitude",  1.0 },
                           { "phase",      0.0 },
                           { "numCycles",  3.0 },  // envelope duration = numCycles / frequency
                       }),
        [] { return new WaveRippleComponent(); });
}
