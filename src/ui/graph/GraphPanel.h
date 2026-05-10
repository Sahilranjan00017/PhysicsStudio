#pragma once

#include <QRect>
#include <QWidget>

class DataLogger;

// ---------------------------------------------------------------------------
// GraphPanel
// Custom QPainter-based live time-series chart.
//
// Layout (top → bottom):
//   ┌─ header (24 px) ──────────────────────── [Clear] ─┐
//   │  legend row: ── channel A   ── channel B           │
//   ├─ plot area ───────────────────────────────────────┤
//   │  dark background, horizontal grid, scrolling time  │
//   └───────────────────────────────────────────────────┘
//
// X axis: last VISIBLE_SECONDS seconds; newest data at right edge.
// Y axis: global min/max of visible data, 10 % padding, min span 0.2.
// Each series is drawn as a anti-aliased polyline in its assigned colour.
// Clicking [Clear] resets all accumulated data.
// ---------------------------------------------------------------------------
class GraphPanel final : public QWidget {
    Q_OBJECT
public:
    explicit GraphPanel(DataLogger* logger, QWidget* parent = nullptr);

    QSize sizeHint()        const override { return QSize(340, 220); }
    QSize minimumSizeHint() const override { return QSize(180, 130); }

public slots:
    void refresh();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;

private:
    // Helpers called from paintEvent.
    void drawEmpty(QPainter& p, const QRect& plot);
    void drawGrid(QPainter& p, const QRect& plot,
                  double tMin, double tMax, double yMin, double yMax);
    void drawSeries(QPainter& p, const QRect& plot,
                    double tMin, double tMax, double yMin, double yMax);
    void drawAxesLabels(QPainter& p, const QRect& plot,
                        double tMin, double tMax,
                        double yMin, double yMax);
    void drawLegend(QPainter& p, int legendY);

    DataLogger* m_logger;
    QRect       m_clearBtnRect;   // hit-test area for the Clear button

    static constexpr double VISIBLE_SECONDS = 15.0;
    static constexpr int    HEADER_H        = 24;
    static constexpr int    LEGEND_H        = 18;
    static constexpr int    MARGIN          =  8;
    static constexpr int    LABEL_W         = 44; // left Y-label margin
};
