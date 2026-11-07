#include "ScreenCapturer.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QUrl>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <QUuid>
#include <QWidget>
#include <QElapsedTimer>
#include <QThreadPool>

static QElapsedTimer s_portalTimer;

ScreenCapturer::ScreenCapturer(QObject *parent)
    : QObject(parent) {
    if (isWayland()) {
        m_portal = std::make_unique<QDBusInterface>(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            QDBusConnection::sessionBus()
        );

        m_dbusConnected = QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            QString(), // Match any request path
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onPortalResponse(uint, QVariantMap))
        );
        if (!m_dbusConnected) {
            qWarning() << "Failed to connect to XDG Desktop Portal Response signal";
        }
    }
}

ScreenCapturer::~ScreenCapturer() {
    if (m_dbusConnected) {
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.portal.Desktop",
            QString(),
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onPortalResponse(uint, QVariantMap))
        );
    }
}

bool ScreenCapturer::isWayland() {
    QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    if (sessionType == "wayland") return true;
    if (!qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty()) return true;
    if (QGuiApplication::platformName().contains("wayland", Qt::CaseInsensitive)) return true;
    return false;
}

void ScreenCapturer::capture() {
    if (m_isCapturing) {
        return; // Debounce rapid key presses
    }
    m_isCapturing = true;

    if (isWayland()) {
        captureWayland();
    } else {
        captureX11();
    }
}

void ScreenCapturer::captureX11() {
    QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        m_isCapturing = false;
        emit captureFailed("未检测到显示屏");
        return;
    }

    QRect totalGeometry;
    for (QScreen *s : screens) {
        totalGeometry = totalGeometry.united(s->geometry());
    }

    QImage virtualScreen(totalGeometry.size(), QImage::Format_ARGB32_Premultiplied);
    virtualScreen.fill(Qt::black);
    QPainter painter(&virtualScreen);

    for (QScreen *s : screens) {
        QPixmap screenPix = s->grabWindow(0);
        painter.drawPixmap(s->geometry().topLeft() - totalGeometry.topLeft(), screenPix);
    }
    painter.end();

    m_isCapturing = false;
    if (virtualScreen.isNull()) {
        emit captureFailed("X11 屏幕截取失败");
    } else {
        emit screenCaptured(virtualScreen);
    }
}

void ScreenCapturer::captureWayland() {
    if (!m_portal || !m_portal->isValid()) {
        qWarning() << "XDG Desktop Portal not found, falling back to X11 grabWindow";
        m_isCapturing = false;
        captureX11();
        return;
    }

    s_portalTimer.restart();

    QString token = QUuid::createUuid().toString().remove('{').remove('}').remove('-');

    QVariantMap options;
    options["handle_token"] = token;
    options["interactive"] = false;

    // Build a non-empty parent_window handle. xdg-desktop-portal-gnome
    // (>= 46) rejects an empty string with "Failed to associate portal
    // window with parent window ''" when launched from custom global shortcuts.
    // On Wayland we pass "wayland:" prefix so it proceeds without requiring a focused window.
    QString parentWindow = "wayland:";

    // Use asynchronous DBus call so the main Qt GUI thread is not blocked
    m_portal->asyncCall("Screenshot", parentWindow, options);
}

void ScreenCapturer::onPortalResponse(uint response, const QVariantMap &results) {
    qint64 portalMs = s_portalTimer.elapsed();
    m_isCapturing = false;

    if (response != 0) {
        emit captureFailed("用户取消或系统拒绝截图请求 (Response Code: " + QString::number(response) + ")");
        return;
    }

    if (!results.contains("uri")) {
        emit captureFailed("未在 Portal 响应中找到图片路径");
        return;
    }

    QElapsedTimer loadTimer;
    loadTimer.start();

    QString uriStr = results.value("uri").toString();
    QUrl url(uriStr);
    QString localPath = url.toLocalFile();

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit captureFailed("无法读取截取的图像文件: " + localPath);
        return;
    }

    // Zero-overhead memory-mapped loading: bypass multiple kernel buffers
    uchar *mapPtr = file.map(0, file.size());
    QImage image;
    if (mapPtr) {
        image.loadFromData(mapPtr, file.size(), "PNG");
        file.unmap(mapPtr);
    } else {
        image.load(&file, "PNG");
    }
    file.close();

    // Clean up temporary image file generated by portal asynchronously
    QThreadPool::globalInstance()->start([localPath]() {
        QFile::remove(localPath);
    });

    qDebug() << "[PERF] Portal roundtrip:" << portalMs << "ms, Image decode:" << loadTimer.elapsed() << "ms";

    if (image.isNull()) {
        emit captureFailed("无法解析截取的图像文件: " + localPath);
    } else {
        emit screenCaptured(image);
    }
}
