#pragma once

#include <QObject>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <QVector>
#include <QString>
#include <memory>

enum class ToolType {
    None,
    Rect,
    Ellipse,
    Arrow,
    Pen,
    Step,
    Text,
    Mosaic
};

struct AnnotationItem {
    virtual ~AnnotationItem() = default;
    virtual void draw(QPainter &painter, const QImage &baseImage) = 0;
};

struct RectItem : public AnnotationItem {
    QRect rect;
    QColor color;
    int width;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct EllipseItem : public AnnotationItem {
    QRect rect;
    QColor color;
    int width;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct ArrowItem : public AnnotationItem {
    QPoint start;
    QPoint end;
    QColor color;
    int width;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct PenItem : public AnnotationItem {
    QVector<QPoint> points;
    QColor color;
    int width;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct StepItem : public AnnotationItem {
    QPoint pos;
    int number;
    QColor color;
    int radius = 14;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct TextItem : public AnnotationItem {
    QPoint pos;
    QString text;
    QColor color;
    int fontSize = 14;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

struct MosaicItem : public AnnotationItem {
    QRect rect;
    int blockSize = 10;
    void draw(QPainter &painter, const QImage &baseImage) override;
};

class AnnotationLayer : public QObject {
    Q_OBJECT
public:
    explicit AnnotationLayer(QObject *parent = nullptr);

    void setToolType(ToolType tool) { m_currentTool = tool; }
    ToolType toolType() const { return m_currentTool; }

    void setColor(const QColor &color) { m_currentColor = color; }
    QColor color() const { return m_currentColor; }

    void setStrokeWidth(int width) { m_strokeWidth = width; }
    int strokeWidth() const { return m_strokeWidth; }

    void startDrawing(const QPoint &pos);
    void updateDrawing(const QPoint &pos);
    void finishDrawing(const QPoint &pos);

    void addText(const QPoint &pos, const QString &text);

    bool isEmpty() const { return m_items.isEmpty() && !m_isDrawing; }
    bool canUndo() const { return !m_items.isEmpty(); }
    void undo();
    void clear();

    void render(QPainter &painter, const QImage &baseImage);
    bool isDrawing() const { return m_isDrawing; }

    void resetStepCounter() { m_stepCounter = 1; }

private:
    ToolType m_currentTool = ToolType::None;
    QColor m_currentColor = QColor(255, 59, 48); // PixPin dynamic red
    int m_strokeWidth = 3;
    int m_stepCounter = 1;
    bool m_isDrawing = false;
    QPoint m_startPoint;
    QPoint m_currentPoint;

    QVector<QPoint> m_currentPenPoints;
    QVector<std::shared_ptr<AnnotationItem>> m_items;
};
