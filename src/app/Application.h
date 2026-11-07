#pragma once

#include <QObject>
#include <QList>
#include <memory>
#include "../capture/ScreenCapturer.h"
#include "../capture/CaptureOverlay.h"
#include "../ui/PinWindow.h"
#include "../ocr/OcrEngine.h"
#include "../ocr/OcrResultDialog.h"
#include "SystemTray.h"

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    void start();
    void triggerCapture();
    void pinFromClipboard();
    void closeAllOverlays();

private:
    void initOverlays();

private slots:
    void onScreenCaptured(const QImage &image);
    void onCaptureFailed(const QString &reason);
    void onPinRequested(const QPixmap &pixmap, const QPoint &pos = QPoint());
    void onOcrRequested(const QPixmap &pixmap);

private:
    std::unique_ptr<ScreenCapturer> m_capturer;
    std::unique_ptr<OcrEngine> m_ocrEngine;
    SystemTray *m_tray = nullptr;
    QList<CaptureOverlay *> m_activeOverlays;
    QList<PinWindow *> m_pinWindows;
};
