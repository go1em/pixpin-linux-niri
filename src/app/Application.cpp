#include "Application.h"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QScreen>
#include <QDebug>

Application::Application(QObject *parent)
    : QObject(parent)
    , m_capturer(std::make_unique<ScreenCapturer>())
    , m_ocrEngine(std::make_unique<OcrEngine>()) {

    connect(m_capturer.get(), &ScreenCapturer::screenCaptured, this, &Application::onScreenCaptured);
    connect(m_capturer.get(), &ScreenCapturer::captureFailed, this, &Application::onCaptureFailed);
}

Application::~Application() {
    closeAllOverlays();
    qDeleteAll(m_activeOverlays);
    m_activeOverlays.clear();
}

void Application::start() {
    m_tray = new SystemTray(this);
    connect(m_tray, &SystemTray::captureRequested, this, &Application::triggerCapture);
    connect(m_tray, &SystemTray::pinClipboardRequested, this, &Application::pinFromClipboard);
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);

    m_tray->show();

    // Pre-create CaptureOverlays for all connected screens so they are ready instantly!
    initOverlays();
}

void Application::initOverlays() {
    QList<QScreen *> screens = QGuiApplication::screens();
    if (m_activeOverlays.size() == screens.size()) {
        bool matches = true;
        for (int i = 0; i < screens.size(); ++i) {
            if (m_activeOverlays[i]->targetScreen() != screens[i]) {
                matches = false;
                break;
            }
        }
        if (matches) return;
    }

    qDeleteAll(m_activeOverlays);
    m_activeOverlays.clear();

    for (QScreen *s : screens) {
        auto *overlay = new CaptureOverlay(s);
        overlay->hide();
        m_activeOverlays.append(overlay);

        connect(overlay, &CaptureOverlay::pinRequested, this, &Application::onPinRequested);
        connect(overlay, &CaptureOverlay::ocrRequested, this, &Application::onOcrRequested);

        // When user starts selecting on one monitor, clear selection on other monitors
        connect(overlay, &CaptureOverlay::selectionStarted, this, [this](CaptureOverlay *active) {
            for (auto *o : m_activeOverlays) {
                if (o != active) {
                    o->clearSelection();
                }
            }
        });

        // When any overlay finishes/closes, close (hide) all overlays
        connect(overlay, &CaptureOverlay::overlayClosed, this, &Application::closeAllOverlays);
    }
}

#include <QElapsedTimer>

static QElapsedTimer s_totalCaptureTimer;

void Application::triggerCapture() {
    s_totalCaptureTimer.restart();
    closeAllOverlays();
    m_capturer->capture();
}

void Application::closeAllOverlays() {
    for (auto *o : m_activeOverlays) {
        o->hide();
    }
}

void Application::pinFromClipboard() {
    QClipboard *clipboard = QApplication::clipboard();
    QPixmap pixmap = clipboard->pixmap();
    if (pixmap.isNull()) {
        QMessageBox::information(nullptr, "PixPin 提示", "剪贴板中没有可用的图像数据。");
        return;
    }
    QPoint pos = QCursor::pos() - QPoint(pixmap.width() / 2, pixmap.height() / 2);
    onPinRequested(pixmap, pos);
}

void Application::onScreenCaptured(const QImage &image) {
    if (image.isNull()) {
        onCaptureFailed("捕获到的屏幕图像为空");
        return;
    }

    initOverlays();

    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty() || m_activeOverlays.size() != screens.size()) {
        onCaptureFailed("未检测到有效显示器");
        return;
    }

    QRect totalGeometry;
    for (QScreen *s : screens) {
        totalGeometry = totalGeometry.united(s->geometry());
    }

    double ratioX = (double)image.width() / totalGeometry.width();
    double ratioY = (double)image.height() / totalGeometry.height();

    for (int i = 0; i < screens.size(); ++i) {
        QScreen *s = screens[i];
        QRect screenGeo = s->geometry();
        QRect rawRect(
            qRound((screenGeo.x() - totalGeometry.x()) * ratioX),
            qRound((screenGeo.y() - totalGeometry.y()) * ratioY),
            qRound(screenGeo.width() * ratioX),
            qRound(screenGeo.height() * ratioY)
        );
        rawRect = rawRect.intersected(image.rect());

        QImage screenRawImage = image.copy(rawRect);
        QPixmap screenRawPixmap = QPixmap::fromImage(screenRawImage);

        auto *overlay = m_activeOverlays[i];
        overlay->resetAndSetImage(screenRawPixmap, screenRawImage);
        overlay->showFullScreen();
        overlay->raise();
        overlay->activateWindow();
    }

    qDebug() << "[PERF] Total time from triggerCapture to overlays shown:" << s_totalCaptureTimer.elapsed() << "ms";
}

void Application::onCaptureFailed(const QString &reason) {
    qWarning() << "Capture failed:" << reason;
    if (m_tray) {
        m_tray->showMessage("PixPin 截图失败", reason, QSystemTrayIcon::Warning, 3000);
    }
}

void Application::onPinRequested(const QPixmap &pixmap, const QPoint &pos) {
    if (pixmap.isNull()) return;

    auto *pinWin = new PinWindow(pixmap);
    if (!pos.isNull()) {
        pinWin->move(pos);
    }
    connect(pinWin, &PinWindow::ocrRequested, this, &Application::onOcrRequested);
    m_pinWindows.append(pinWin);

    connect(pinWin, &PinWindow::destroyed, [this, pinWin]() {
        m_pinWindows.removeAll(pinWin);
    });

    pinWin->show();
}

void Application::onOcrRequested(const QPixmap &pixmap) {
    if (pixmap.isNull()) return;

    QString recognized = m_ocrEngine->recognizeText(pixmap);
    bool hasText = !recognized.trimmed().isEmpty();
    if (!hasText) {
        recognized = "(未识别出清晰文本)";
    } else {
        // Automatically copy recognized text directly to clipboard
        QApplication::clipboard()->setText(recognized);
    }

    auto *dialog = new OcrResultDialog(recognized, hasText);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
