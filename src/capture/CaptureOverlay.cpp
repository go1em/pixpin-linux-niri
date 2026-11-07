#include "CaptureOverlay.h"
#include "../utils/ImageUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QInputDialog>
#include <QWindow>
#include <QCursor>
#include <algorithm>

CaptureOverlay::CaptureOverlay(QScreen *screen, const QPixmap &screenRawPixmap, const QImage &screenRawImage, QWidget *parent)
    : QWidget(parent, Qt::BypassWindowManagerHint | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool)
    , m_screen(screen) {
    // Note: Do NOT set WA_DeleteOnClose, so this overlay can be reused instantly without widget reconstruction!
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setMouseTracking(true);

    winId();
    if (windowHandle() && m_screen) {
        windowHandle()->setScreen(m_screen);
    }
    if (m_screen) {
        setGeometry(m_screen->geometry());
    }

    setupUI();

    if (!screenRawPixmap.isNull()) {
        resetAndSetImage(screenRawPixmap, screenRawImage);
    }
}

void CaptureOverlay::resetAndSetImage(const QPixmap &screenRawPixmap, const QImage &screenRawImage) {
    m_rawPixmap = screenRawPixmap;
    if (!screenRawImage.isNull()) {
        m_rawImage = screenRawImage;
    } else {
        m_rawImage = m_rawPixmap.toImage();
    }

    if (m_screen) {
        setGeometry(m_screen->geometry());
        m_scaleX = (double)m_rawPixmap.width() / m_screen->geometry().width();
        m_scaleY = (double)m_rawPixmap.height() / m_screen->geometry().height();
    }

    // Pre-scale screenshot once to logical geometry for instant 1:1 hardware blit
    QSize logicalSize = m_screen ? m_screen->geometry().size() : size();
    if (m_rawPixmap.size() != logicalSize) {
        m_scaledPixmap = m_rawPixmap.scaled(logicalSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    } else {
        m_scaledPixmap = m_rawPixmap;
    }

    m_selectedRect = QRect();
    m_selectionShape = SelectionShape::Rectangle;
    m_state = CaptureState::Idle;
    m_activeHandle = ResizeHandle::None;
    setCursor(Qt::CrossCursor);

    if (m_annotations) {
        m_annotations->clear();
        m_annotations->resetStepCounter();
        m_annotations->setToolType(ToolType::None);
    }
    if (m_toolbar) {
        m_toolbar->resetTools();
        m_toolbar->hide();
    }
    if (m_magnifier) {
        m_magnifier->show();
    }
    update();
}

void CaptureOverlay::setupUI() {
    m_magnifier = new MagnifierWidget(this);
    m_magnifier->show();

    m_toolbar = new ToolBarWidget(this);
    m_toolbar->hide();

    m_annotations = new AnnotationLayer(this);

    // Toolbar connections
    connect(m_toolbar, &ToolBarWidget::toolSelected, [this](ToolType tool) {
        m_annotations->setToolType(tool);
        if (tool != ToolType::None) {
            m_state = CaptureState::Annotating;
            setCursor(Qt::CrossCursor);
        } else {
            m_state = CaptureState::Selected;
            setCursor(Qt::ArrowCursor);
        }
    });

    connect(m_toolbar, &ToolBarWidget::colorChanged, [this](const QColor &color) {
        m_annotations->setColor(color);
    });

    connect(m_toolbar, &ToolBarWidget::strokeWidthChanged, [this](int width) {
        m_annotations->setStrokeWidth(width);
    });

    connect(m_toolbar, &ToolBarWidget::undoTriggered, [this]() {
        m_annotations->undo();
        update();
    });

    connect(m_toolbar, &ToolBarWidget::selectionShapeChanged, this, &CaptureOverlay::setSelectionShape);
    connect(m_toolbar, &ToolBarWidget::copyTriggered, this, &CaptureOverlay::onCopy);
    connect(m_toolbar, &ToolBarWidget::saveTriggered, this, &CaptureOverlay::onSave);
    connect(m_toolbar, &ToolBarWidget::pinTriggered, this, &CaptureOverlay::onPin);
    connect(m_toolbar, &ToolBarWidget::ocrTriggered, this, &CaptureOverlay::onOcr);
    connect(m_toolbar, &ToolBarWidget::closeTriggered, [this]() {
        emit overlayClosed();
    });
}

void CaptureOverlay::setSelectionShape(SelectionShape shape) {
    if (m_selectionShape != shape) {
        m_selectionShape = shape;
        if (m_magnifier) {
            m_magnifier->setSelectionShape(shape);
        }
        if (m_toolbar) {
            m_toolbar->setSelectionShape(shape);
        }
        if (m_selectedRect.isValid() && !m_selectedRect.isEmpty()) {
            update(m_selectedRect.adjusted(-15, -40, 15, 15).intersected(rect()));
        }
    }
}

void CaptureOverlay::toggleSelectionShape() {
    setSelectionShape(m_selectionShape == SelectionShape::Rectangle ? SelectionShape::Ellipse : SelectionShape::Rectangle);
}

void CaptureOverlay::clearSelection() {
    if (m_state != CaptureState::Idle) {
        m_state = CaptureState::Idle;
        QRect old = m_selectedRect;
        m_selectedRect = QRect();
        m_toolbar->hide();
        m_magnifier->show();
        if (old.isValid() && !old.isEmpty()) {
            update(old.adjusted(-15, -40, 15, 15).intersected(rect()));
        }
    }
}

ResizeHandle CaptureOverlay::hitTest(const QPoint &pos) const {
    if (m_selectedRect.isNull() || !m_selectedRect.isValid()) return ResizeHandle::None;

    QRect r = m_selectedRect.normalized();

    if (getHandleRect(r.topLeft()).contains(pos)) return ResizeHandle::TopLeft;
    if (getHandleRect(QPoint(r.center().x(), r.top())).contains(pos)) return ResizeHandle::Top;
    if (getHandleRect(r.topRight()).contains(pos)) return ResizeHandle::TopRight;
    if (getHandleRect(QPoint(r.right(), r.center().y())).contains(pos)) return ResizeHandle::Right;
    if (getHandleRect(r.bottomRight()).contains(pos)) return ResizeHandle::BottomRight;
    if (getHandleRect(QPoint(r.center().x(), r.bottom())).contains(pos)) return ResizeHandle::Bottom;
    if (getHandleRect(r.bottomLeft()).contains(pos)) return ResizeHandle::BottomLeft;
    if (getHandleRect(QPoint(r.left(), r.center().y())).contains(pos)) return ResizeHandle::Left;

    return ResizeHandle::None;
}

QRect CaptureOverlay::getHandleRect(const QPoint &center) const {
    return QRect(center.x() - m_handleSize / 2, center.y() - m_handleSize / 2, m_handleSize, m_handleSize);
}

QRect CaptureOverlay::getBadgeRect(const QRect &r) const {
    if (!r.isValid() || r.isEmpty()) return QRect();

    static const QFont badgeFont("Sans", 9, QFont::Bold);
    static const QFontMetrics badgeFm(badgeFont);

    QString dimText;
    if (m_selectionShape == SelectionShape::Rectangle) {
        dimText = QString("%1 × %2").arg(r.width()).arg(r.height());
    } else if (r.width() == r.height()) {
        dimText = QString("圆 ⌀ %1").arg(r.width());
    } else {
        dimText = QString("椭圆 %1 × %2").arg(r.width()).arg(r.height());
    }

    int textW = badgeFm.horizontalAdvance(dimText) + 12;
    int textH = badgeFm.height() + 6;

    int badgeY = r.top() - textH - 4;
    if (badgeY < 4) badgeY = r.top() + 4;

    return QRect(r.left(), badgeY, textW, textH);
}

QRegion CaptureOverlay::calculateDirtyRegion(const QRect &oldR, const QRect &newR) const {
    QRect r1 = oldR.normalized();
    QRect r2 = newR.normalized();

    if (!r1.isValid() || r1.isEmpty()) {
        if (!r2.isValid() || r2.isEmpty()) return QRegion();
        return QRegion(r2.adjusted(-15, -40, 15, 15).intersected(rect()));
    }
    if (!r2.isValid() || r2.isEmpty()) {
        return QRegion(r1.adjusted(-15, -40, 15, 15).intersected(rect()));
    }

    if (m_selectionShape != SelectionShape::Rectangle) {
        // For ellipse mode, union old and new bounding box
        return QRegion(r1.united(r2).adjusted(-15, -40, 15, 15).intersected(rect()));
    }

    // High performance delta region for rectangle mode:
    QRegion dirty;

    // 1. Interior transition strips:
    dirty += QRegion(r1).xored(QRegion(r2));

    // 2. Borders: 4px thickness to cover 2px border line and antialiasing margins
    dirty += QRegion(r1.adjusted(-4, -4, 4, 4)).subtracted(r1.adjusted(4, 4, -4, -4));
    dirty += QRegion(r2.adjusted(-4, -4, 4, 4)).subtracted(r2.adjusted(4, 4, -4, -4));

    // 3. Size badges:
    QRect oldBadge = getBadgeRect(r1);
    QRect newBadge = getBadgeRect(r2);
    if (!oldBadge.isEmpty()) dirty += oldBadge.adjusted(-3, -3, 3, 3);
    if (!newBadge.isEmpty()) dirty += newBadge.adjusted(-3, -3, 3, 3);

    // 4. Handles if in Resizing or Selected state
    if (m_state == CaptureState::Resizing || m_state == CaptureState::Selected) {
        auto addHandles = [this, &dirty](const QRect &r) {
            QList<QPoint> handles = {
                r.topLeft(),
                QPoint(r.center().x(), r.top()),
                r.topRight(),
                QPoint(r.right(), r.center().y()),
                r.bottomRight(),
                QPoint(r.center().x(), r.bottom()),
                r.bottomLeft(),
                QPoint(r.left(), r.center().y())
            };
            for (const QPoint &pt : handles) {
                dirty += getHandleRect(pt).adjusted(-2, -2, 2, 2);
            }
        };
        addHandles(r1);
        addHandles(r2);
    }

    return dirty.intersected(rect());
}

void CaptureOverlay::updateCursorForHandle(ResizeHandle handle) {
    switch (handle) {
    case ResizeHandle::TopLeft:
    case ResizeHandle::BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case ResizeHandle::TopRight:
    case ResizeHandle::BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case ResizeHandle::Top:
    case ResizeHandle::Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case ResizeHandle::Left:
    case ResizeHandle::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    default:
        if (m_state == CaptureState::Selected) {
            setCursor(Qt::SizeAllCursor);
        } else if (m_state == CaptureState::Annotating) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        break;
    }
}

void CaptureOverlay::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    const QRegion dirtyRegion = event->region();
    const QColor dimColor(0, 0, 0, 115);

    // 1. Draw only the dirty background strips (avoids blitting millions of unchanged pixels)
    for (const QRect &clip : dirtyRegion) {
        p.drawPixmap(clip, m_scaledPixmap, clip);

        if (!m_selectedRect.isValid() || m_selectedRect.isEmpty()) {
            p.fillRect(clip, dimColor);
        } else {
            QRect r = m_selectedRect.normalized();
            if (m_selectionShape == SelectionShape::Rectangle) {
                // Fill 4 surrounding bands intersecting clip
                if (r.top() > clip.top()) {
                    p.fillRect(QRect(clip.left(), clip.top(), clip.width(), r.top() - clip.top()), dimColor);
                }
                if (r.bottom() < clip.bottom()) {
                    p.fillRect(QRect(clip.left(), r.bottom() + 1, clip.width(), clip.bottom() - r.bottom()), dimColor);
                }
                int midTop = std::max(clip.top(), r.top());
                int midBottom = std::min(clip.bottom(), r.bottom());
                if (midTop <= midBottom) {
                    int midHeight = midBottom - midTop + 1;
                    if (r.left() > clip.left()) {
                        p.fillRect(QRect(clip.left(), midTop, r.left() - clip.left(), midHeight), dimColor);
                    }
                    if (r.right() < clip.right()) {
                        p.fillRect(QRect(r.right() + 1, midTop, clip.right() - r.right(), midHeight), dimColor);
                    }
                }
            } else {
                // Ellipse shape with fast OddEvenFill rule
                QPainterPath path;
                path.setFillRule(Qt::OddEvenFill);
                path.addRect(clip);
                path.addEllipse(r);
                p.fillPath(path, dimColor);
            }
        }
    }

    if (m_selectedRect.isValid() && !m_selectedRect.isEmpty()) {
        QRect r = m_selectedRect.normalized();

        // 2. Render annotations inside selection (only if any exist!)
        if (m_annotations && !m_annotations->isEmpty()) {
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            if (m_selectionShape == SelectionShape::Rectangle) {
                p.setClipRect(r);
            } else {
                QPainterPath ellipsePath;
                ellipsePath.addEllipse(r);
                p.setClipPath(ellipsePath);
            }
            m_annotations->render(p, m_rawImage);
            p.restore();
        }

        // 3. Selection Border (use crisp pixel-perfect lines for rectangle, anti-aliased for ellipse)
        p.setPen(QPen(QColor(0, 122, 255), 2));
        p.setBrush(Qt::NoBrush);
        if (m_selectionShape == SelectionShape::Rectangle) {
            p.setRenderHint(QPainter::Antialiasing, false);
            p.drawRect(r);
        } else {
            p.setRenderHint(QPainter::Antialiasing, true);
            p.drawEllipse(r);
        }

        // 4. Control Points (Handles)
        if (m_state == CaptureState::Selected || m_state == CaptureState::Resizing) {
            p.setRenderHint(QPainter::Antialiasing, false);
            p.setPen(QPen(QColor(0, 122, 255), 1.5));
            p.setBrush(Qt::white);

            QList<QPoint> handles = {
                r.topLeft(),
                QPoint(r.center().x(), r.top()),
                r.topRight(),
                QPoint(r.right(), r.center().y()),
                r.bottomRight(),
                QPoint(r.center().x(), r.bottom()),
                r.bottomLeft(),
                QPoint(r.left(), r.center().y())
            };

            for (const QPoint &pt : handles) {
                p.drawRect(getHandleRect(pt));
            }
        }

        // 5. Dimension badge
        QRect badgeRect = getBadgeRect(r);
        if (!badgeRect.isEmpty()) {
            static const QFont badgeFont("Sans", 9, QFont::Bold);
            p.setFont(badgeFont);

            QString dimText;
            if (m_selectionShape == SelectionShape::Rectangle) {
                dimText = QString("%1 × %2").arg(r.width()).arg(r.height());
            } else if (r.width() == r.height()) {
                dimText = QString("圆 ⌀ %1").arg(r.width());
            } else {
                dimText = QString("椭圆 %1 × %2").arg(r.width()).arg(r.height());
            }

            p.fillRect(badgeRect, QColor(20, 20, 20, 210));
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, dimText);
        }
    }
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    m_startPos = event->pos();
    m_lastMousePos = event->pos();

    if (m_state == CaptureState::Annotating) {
        if (m_annotations->toolType() == ToolType::Text) {
            bool ok;
            QString text = QInputDialog::getText(this, "添加文本标注", "请输入标注文本:", QLineEdit::Normal, "", &ok);
            if (ok && !text.isEmpty()) {
                m_annotations->addText(event->pos(), text);
                update(m_selectedRect.normalized());
            }
        } else {
            m_annotations->startDrawing(event->pos());
        }
        return;
    }

    if (m_state == CaptureState::Selected) {
        m_activeHandle = hitTest(event->pos());
        if (m_activeHandle != ResizeHandle::None) {
            m_state = CaptureState::Resizing;
            return;
        }

        if (m_selectedRect.normalized().contains(event->pos())) {
            m_state = CaptureState::Moving;
            return;
        }

        // Click outside restarts selection
        QRect old = m_selectedRect;
        m_state = CaptureState::Selecting;
        m_selectedRect = QRect(event->pos(), QSize(0, 0));
        m_toolbar->hide();
        m_magnifier->hide();
        emit selectionStarted(this);
        if (old.isValid() && !old.isEmpty()) {
            update(old.adjusted(-15, -40, 15, 15).intersected(rect()));
        }
        return;
    }

    if (m_state == CaptureState::Idle) {
        m_state = CaptureState::Selecting;
        m_selectedRect = QRect(event->pos(), QSize(0, 0));
        m_magnifier->hide();
        emit selectionStarted(this);
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    // In Idle mode, update only the 160x200 magnifier widget.
    // NEVER call update() on the full screen here to ensure 100% smooth mouse movement!
    if (m_state == CaptureState::Idle) {
        if (m_magnifier) {
            m_magnifier->updatePositionAndData(pos, m_rawImage, m_scaleX, m_scaleY);
        }
        return;
    }

    if (m_state == CaptureState::Annotating) {
        m_annotations->updateDrawing(pos);
        update(m_selectedRect.normalized());
        return;
    }

    if (m_state == CaptureState::Selecting) {
        QRect oldRect = m_selectedRect;
        QPoint curPos = pos;
        if (event->modifiers() & Qt::ShiftModifier) {
            // Constrain to 1:1 aspect ratio (Square or Circle)
            int dx = curPos.x() - m_startPos.x();
            int dy = curPos.y() - m_startPos.y();
            int side = qMax(qAbs(dx), qAbs(dy));
            int signX = (dx >= 0) ? 1 : -1;
            int signY = (dy >= 0) ? 1 : -1;
            curPos = QPoint(m_startPos.x() + signX * side, m_startPos.y() + signY * side);
        }
        m_selectedRect = QRect(m_startPos, curPos).normalized();
        QRegion dirty = calculateDirtyRegion(oldRect, m_selectedRect);
        update(dirty);
        return;
    }

    if (m_state == CaptureState::Moving) {
        QRect oldRect = m_selectedRect;
        QPoint delta = pos - m_lastMousePos;
        m_selectedRect.translate(delta);
        m_lastMousePos = pos;
        m_toolbar->updatePosition(m_selectedRect, rect());
        QRegion dirty = calculateDirtyRegion(oldRect, m_selectedRect);
        update(dirty);
        return;
    }

    if (m_state == CaptureState::Resizing) {
        QRect oldRect = m_selectedRect;
        QRect r = m_selectedRect.normalized();
        switch (m_activeHandle) {
        case ResizeHandle::TopLeft: r.setTopLeft(pos); break;
        case ResizeHandle::Top: r.setTop(pos.y()); break;
        case ResizeHandle::TopRight: r.setTopRight(pos); break;
        case ResizeHandle::Right: r.setRight(pos.x()); break;
        case ResizeHandle::BottomRight: r.setBottomRight(pos); break;
        case ResizeHandle::Bottom: r.setBottom(pos.y()); break;
        case ResizeHandle::BottomLeft: r.setBottomLeft(pos); break;
        case ResizeHandle::Left: r.setLeft(pos.x()); break;
        default: break;
        }
        m_selectedRect = r.normalized();
        m_toolbar->updatePosition(m_selectedRect, rect());
        QRegion dirty = calculateDirtyRegion(oldRect, m_selectedRect);
        update(dirty);
        return;
    }

    if (m_state == CaptureState::Selected) {
        ResizeHandle h = hitTest(pos);
        updateCursorForHandle(h);
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;

    if (m_state == CaptureState::Annotating) {
        m_annotations->finishDrawing(event->pos());
        update(m_selectedRect.normalized());
        return;
    }

    if (m_state == CaptureState::Selecting || m_state == CaptureState::Resizing || m_state == CaptureState::Moving) {
        m_selectedRect = m_selectedRect.normalized();

        // Filter out accidental clicks
        if (m_selectedRect.width() > 10 && m_selectedRect.height() > 10) {
            m_state = CaptureState::Selected;
            m_magnifier->hide();
            m_toolbar->updatePosition(m_selectedRect, rect());
            m_toolbar->show();
            setCursor(Qt::ArrowCursor);
            update(m_selectedRect.adjusted(-15, -40, 15, 15).intersected(rect()));
        } else {
            m_state = CaptureState::Idle;
            QRect old = m_selectedRect;
            m_selectedRect = QRect();
            m_toolbar->hide();
            m_magnifier->show();
            if (old.isValid() && !old.isEmpty()) {
                update(old.adjusted(-15, -40, 15, 15).intersected(rect()));
            }
        }
    }
}

void CaptureOverlay::enterEvent(QEnterEvent *) {
    if (m_state == CaptureState::Idle && m_magnifier) {
        m_magnifier->show();
    }
}

void CaptureOverlay::leaveEvent(QEvent *) {
    if (m_state == CaptureState::Idle && m_magnifier) {
        m_magnifier->hide();
    }
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_selectedRect.normalized().contains(event->pos())) {
        onCopy();
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit overlayClosed();
    } else if (event->key() == Qt::Key_Tab) {
        toggleSelectionShape();
    } else if (event->key() == Qt::Key_R && !(event->modifiers() & Qt::ControlModifier)) {
        setSelectionShape(SelectionShape::Rectangle);
    } else if (event->key() == Qt::Key_E && !(event->modifiers() & Qt::ControlModifier)) {
        setSelectionShape(SelectionShape::Ellipse);
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        onCopy();
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier)) {
        onCopy();
    } else if (event->key() == Qt::Key_S && (event->modifiers() & Qt::ControlModifier)) {
        onSave();
    } else if (event->key() == Qt::Key_P && (event->modifiers() & Qt::ControlModifier)) {
        onPin();
    } else if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ShiftModifier)) {
        if (!m_selectedRect.isValid() || m_selectedRect.isEmpty()) {
            m_selectedRect = rect();
        }
        onOcr();
    } else if (event->key() == Qt::Key_Z && (event->modifiers() & Qt::ControlModifier)) {
        m_annotations->undo();
        update();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void CaptureOverlay::wheelEvent(QWheelEvent *event) {
    if (m_state != CaptureState::Annotating) {
        int delta = event->angleDelta().y();
        if (delta == 0) {
            delta = event->angleDelta().x();
        }
        if (delta != 0) {
            m_wheelAccumulator += delta;
            if (qAbs(m_wheelAccumulator) >= 120) {
                toggleSelectionShape();
                m_wheelAccumulator = 0;
            }
            event->accept();
            return;
        }
    }
    QWidget::wheelEvent(event);
}

QPixmap CaptureOverlay::getComposedPixmap(bool highRes) const {
    QRect r = m_selectedRect.normalized();
    if (!r.isValid() || r.isEmpty()) return QPixmap();

    // Map logical rect r to raw physical coordinates
    QRect rawRect(
        qRound(r.x() * m_scaleX),
        qRound(r.y() * m_scaleY),
        qRound(r.width() * m_scaleX),
        qRound(r.height() * m_scaleY)
    );
    rawRect = rawRect.intersected(m_rawPixmap.rect());

    QPixmap rawCrop = m_rawPixmap.copy(rawRect);

    if (!highRes) {
        // Normal resolution (e.g. for Pinning sticker)
        QImage resultImage(r.size(), QImage::Format_ARGB32_Premultiplied);
        resultImage.fill(Qt::transparent);

        QPainter p(&resultImage);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        if (m_selectionShape == SelectionShape::Ellipse) {
            QPainterPath path;
            path.addEllipse(QRectF(0, 0, r.width(), r.height()));
            p.setClipPath(path);
        }

        p.drawPixmap(0, 0, rawCrop.scaled(r.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        p.translate(-r.topLeft());
        const_cast<AnnotationLayer *>(m_annotations)->render(p, m_rawImage);
        p.end();

        return QPixmap::fromImage(resultImage);
    }

    // High resolution (for Saving as Transparent PNG, Copying, and OCR)
    QImage resultImage(rawCrop.size(), QImage::Format_ARGB32_Premultiplied);
    resultImage.fill(Qt::transparent);

    QPainter p(&resultImage);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (m_selectionShape == SelectionShape::Ellipse) {
        QPainterPath path;
        path.addEllipse(QRectF(0, 0, rawCrop.width(), rawCrop.height()));
        p.setClipPath(path);
    }

    p.drawPixmap(0, 0, rawCrop);
    p.scale(m_scaleX, m_scaleY);
    p.translate(-r.topLeft());
    const_cast<AnnotationLayer *>(m_annotations)->render(p, m_rawImage);
    p.end();

    return QPixmap::fromImage(resultImage);
}

void CaptureOverlay::onCopy() {
    QPixmap result = getComposedPixmap(true);
    if (!result.isNull()) {
        QApplication::clipboard()->setPixmap(result);
    }
    emit overlayClosed();
}

void CaptureOverlay::onSave() {
    QPixmap result = getComposedPixmap(true);
    if (result.isNull()) return;

    m_toolbar->hide();
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString fileName = QString("PixPin_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString filePath = QFileDialog::getSaveFileName(this, "保存截图", defaultPath + "/" + fileName, "PNG 图像 (*.png);;JPEG 图像 (*.jpg)");

    if (!filePath.isEmpty()) {
        if (!filePath.endsWith(".png", Qt::CaseInsensitive) && !filePath.endsWith(".jpg", Qt::CaseInsensitive) && !filePath.endsWith(".jpeg", Qt::CaseInsensitive)) {
            filePath += ".png";
        }
        result.save(filePath);
    }
    emit overlayClosed();
}

void CaptureOverlay::onPin() {
    QPixmap result = getComposedPixmap(false);
    if (!result.isNull()) {
        QRect r = m_selectedRect.normalized();
        QPoint globalPos = m_screen->geometry().topLeft() + r.topLeft();
        emit pinRequested(result, globalPos);
    }
    emit overlayClosed();
}

void CaptureOverlay::onOcr() {
    // Send 100% native resolution image for maximum OCR accuracy
    QPixmap result = getComposedPixmap(true);
    if (!result.isNull()) {
        emit ocrRequested(result);
    }
    emit overlayClosed();
}
