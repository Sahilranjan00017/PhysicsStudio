#include "ui/graph/GraphPanel.h"
#include "ui/graph/DataLogger.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>

// ---------------------------------------------------------------------------
// Colour constants
// ---------------------------------------------------------------------------
namespace {

const QColor BG_DARK   { 22,  26,  34 };   // plot background
const QColor BG_HEADER { 30,  34,  44 };   // header strip
const QColor GRID_CLR  { 55,  62,  76 };   // grid lines
const QColor TEXT_CLR  { 180, 190, 200 };  // axis labels / header text
const QColor BTN_CLR   { 70,  80,  95 };   // Clear button fill
const QColor BTN_TEXT  { 210, 220, 230 };  // Clear button text

} // namespace

// ---------------------------------------------------------------------------
GraphPanel::GraphPanel(DataLogger* logger, QWidget* parent)
    : QWidget(parent), m_logger(logger)
{
    setMinimumSize(minimumSizeHint());
    connect(logger, &DataLogger::seriesUpdated, this, &GraphPanel::refresh);
}

void GraphPanel::refresh()
{
    update();
}

// ---------------------------------------------------------------------------
void GraphPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int W = width();
    const int H = height();

    // ── Header strip ─────────────────────────────────────────────────────
    p.fillRect(0, 0, W, HEADER_H, BG_HEADER);
    p.setPen(TEXT_CLR);
    p.setFont(QFont("sans-serif", 8, QFont::Bold));
    p.drawText(QRect(MARGIN, 0, W - 100, HEADER_H),
               Qt::AlignVCenter | Qt::AlignLeft, "Live Measurements");

    // Clear button
    m_clearBtnRect = QRect(W - 52, 4, 46, HEADER_H - 8);
    p.fillRect(m_clearBtnRect, BTN_CLR);
    p.setPen(BTN_TEXT);
    p.setFont(QFont("sans-serif", 7));
    p.drawText(m_clearBtnRect, Qt::AlignCenter, "Clear");

    // ── Legend row ───────────────────────────────────────────────────────
    const int legendY = HEADER_H;
    p.fillRect(0, legendY, W, LEGEND_H, BG_DARK);
    drawLegend(p, legendY);

    // ── Plot area ────────────────────────────────────────────────────────
    const QRect plot(LABEL_W,
                     HEADER_H + LEGEND_H,
                     W - LABEL_W - MARGIN,
                     H - HEADER_H - LEGEND_H - MARGIN);
    if (plot.width() < 20 || plot.height() < 20) return;

    p.fillRect(plot, BG_DARK);

    const auto& series = m_logger->series();
    if (series.isEmpty()) {
        drawEmpty(p, plot);
        return;
    }

    // Compute time window.
    const double tMax = m_logger->currentTime();
    const double tMin = tMax - VISIBLE_SECONDS;

    // Compute unified Y range across all series.
    double yMin =  1e30;
    double yMax = -1e30;
    for (const auto& ds : series) {
        for (const QPointF& pt : ds.points) {
            if (pt.x() < tMin) continue;
            yMin = qMin(yMin, pt.y());
            yMax = qMax(yMax, pt.y());
        }
    }
    if (yMin > yMax) { yMin = -1.0; yMax = 1.0; }

    // Ensure a minimum visible span and add 10 % padding.
    const double span = qMax(yMax - yMin, 0.2);
    const double pad  = span * 0.10;
    yMin -= pad;
    yMax += pad;

    drawGrid(p, plot, tMin, tMax, yMin, yMax);
    drawSeries(p, plot, tMin, tMax, yMin, yMax);
    drawAxesLabels(p, plot, tMin, tMax, yMin, yMax);
}

// ---------------------------------------------------------------------------
void GraphPanel::drawEmpty(QPainter& p, const QRect& plot)
{
    p.setPen(GRID_CLR);
    p.setFont(QFont("sans-serif", 8));
    p.drawText(plot, Qt::AlignCenter, "No data\nPress Play to start simulation");
}

// ---------------------------------------------------------------------------
void GraphPanel::drawGrid(QPainter& p, const QRect& plot,
                          double /*tMin*/, double /*tMax*/,
                          double /*yMin*/, double /*yMax*/)
{
    p.setPen(QPen(GRID_CLR, 1, Qt::DotLine));

    // 4 horizontal grid lines at 20 / 40 / 60 / 80 % of plot height.
    for (int i = 1; i <= 4; ++i) {
        const int y = plot.top() + plot.height() * i / 5;
        p.drawLine(plot.left(), y, plot.right(), y);
    }

    // Vertical grid lines every 3 seconds.
    constexpr double TICK_INTERVAL = 3.0; // seconds
    for (double rel = 0.0; rel <= VISIBLE_SECONDS; rel += TICK_INTERVAL) {
        const int x = plot.right() - static_cast<int>(rel / VISIBLE_SECONDS * plot.width());
        p.drawLine(x, plot.top(), x, plot.bottom());
    }
}

// ---------------------------------------------------------------------------
void GraphPanel::drawSeries(QPainter& p, const QRect& plot,
                             double tMin, double tMax,
                             double yMin, double yMax)
{
    const double tSpan = (tMax > tMin) ? tMax - tMin : 1.0;
    const double ySpan = (yMax > yMin) ? yMax - yMin : 1.0;

    auto toScreen = [&](const QPointF& pt) -> QPointF {
        const double rx = (pt.x() - tMin) / tSpan;
        const double ry = (pt.y() - yMin) / ySpan;
        return QPointF(plot.left() + rx * plot.width(),
                       plot.bottom() - ry * plot.height());
    };

    p.save();
    p.setClipRect(plot);

    for (const auto& ds : m_logger->series()) {
        if (ds.points.size() < 2) continue;

        QPainterPath path;
        bool started = false;

        for (const QPointF& pt : ds.points) {
            if (pt.x() < tMin - 0.1) continue; // skip far-past data
            const QPointF sp = toScreen(pt);
            if (!started) {
                path.moveTo(sp);
                started = true;
            } else {
                path.lineTo(sp);
            }
        }

        p.setPen(QPen(ds.color, 1.5f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }

    p.restore();
}

// ---------------------------------------------------------------------------
void GraphPanel::drawAxesLabels(QPainter& p, const QRect& plot,
                                 double /*tMin*/, double tMax,
                                 double yMin, double yMax)
{
    p.setPen(TEXT_CLR);
    p.setFont(QFont("monospace", 7));

    // Y-axis labels (5 positions: bottom, 25%, 50%, 75%, top).
    for (int i = 0; i <= 4; ++i) {
        const double frac = i / 4.0;
        const double val  = yMin + frac * (yMax - yMin);
        const int    y    = plot.bottom() - static_cast<int>(frac * plot.height());
        p.drawText(QRect(0, y - 8, LABEL_W - 2, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(val, 'g', 3));
    }

    // X-axis time labels (every 3 s).
    constexpr double TICK = 3.0;
    for (double rel = 0.0; rel <= VISIBLE_SECONDS; rel += TICK) {
        const double t   = tMax - rel;
        const int    x   = plot.right() - static_cast<int>(rel / VISIBLE_SECONDS * plot.width());
        const QString lbl = QString("%1s").arg(t, 0, 'f', 0);
        p.drawText(QRect(x - 20, plot.bottom() + 1, 40, MARGIN - 1),
                   Qt::AlignCenter, lbl);
    }
}

// ---------------------------------------------------------------------------
void GraphPanel::drawLegend(QPainter& p, int legendY)
{
    const auto& series = m_logger->series();
    if (series.isEmpty()) return;

    p.setFont(QFont("sans-serif", 7));
    int x = LABEL_W;

    for (const auto& ds : series) {
        // Coloured line swatch.
        p.setPen(QPen(ds.color, 2));
        const int midY = legendY + LEGEND_H / 2;
        p.drawLine(x, midY, x + 14, midY);

        // Label text.
        p.setPen(TEXT_CLR);
        const QRect textRect(x + 18, legendY, 140, LEGEND_H);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, ds.label);

        x += 18 + p.fontMetrics().horizontalAdvance(ds.label) + 14;
        if (x + 60 > width()) break; // don't overflow
    }
}

// ---------------------------------------------------------------------------
void GraphPanel::mousePressEvent(QMouseEvent* ev)
{
    if (m_clearBtnRect.contains(ev->pos())) {
        m_logger->clearData();
        update();
    }
    QWidget::mousePressEvent(ev);
}
