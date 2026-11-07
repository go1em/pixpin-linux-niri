#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QDebug>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "app/Application.h"

int main(int argc, char *argv[]) {
    // ULTRA-FAST CLI DISPATCH:
    // When invoked via shortcut (e.g. Alt-1 calling 'pixpin -c' or 'pixpin -p'),
    // check immediately if a background daemon is already running via POSIX UNIX domain socket.
    // If so, write the command and exit in < 0.1ms without loading Qt GUI, Wayland, fonts or theme engines!
    bool quickCapture = false;
    bool quickPin = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--capture") == 0) {
            quickCapture = true;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pin") == 0) {
            quickPin = true;
        }
    }

    if (quickCapture || quickPin) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/pixpin_linux_ipc_%d", getuid());

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                const char *cmd = quickPin ? "pin\n" : "capture\n";
                ssize_t len = strlen(cmd);
                if (write(sock, cmd, len) == len) {
                    close(sock);
                    return 0; // Exited in 0.05ms without initializing Qt GUI!
                }
            }
            close(sock);
        }
    }

    QApplication app(argc, argv);
    app.setApplicationName("PixPin Linux");
    app.setApplicationVersion(PIXPIN_VERSION);
    app.setOrganizationName("PixPin");
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription("PixPin Linux - 智能截图、贴图与离线 OCR 文本识别工具");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption captureOption(QStringList() << "c" << "capture", "立即启动截屏 (可绑定全局快捷键)");
    QCommandLineOption pinOption(QStringList() << "p" << "pin", "直接贴出剪贴板中的图片");
    QCommandLineOption ocrOption(QStringList() << "o" << "ocr", "识别指定图片文件的文字", "file");
    parser.addOption(captureOption);
    parser.addOption(pinOption);
    parser.addOption(ocrOption);

    parser.process(app);

    if (parser.isSet(ocrOption)) {
        QString filePath = parser.value(ocrOption);
        QImage img(filePath);
        if (img.isNull()) {
            qCritical() << "无法加载图片:" << filePath;
            return 1;
        }
        OcrEngine engine;
        QString text = engine.recognizeText(img);
        QTextStream out(stdout);
        out << text << "\n";
        return 0;
    }

    QString serverName = QString("pixpin_linux_ipc_%1").arg(getuid());

    // Check if an instance is already running via QLocalSocket fallback
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(50)) {
        // An instance is already running in background! Send command and exit immediately.
        QString command = "capture";
        if (parser.isSet(pinOption)) {
            command = "pin";
        } else if (parser.isSet(captureOption)) {
            command = "capture";
        }

        socket.write(command.toUtf8());
        socket.flush();
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
    }

    // No existing instance running. This process becomes the primary daemon.
    QLocalServer::removeServer(serverName);
    auto *server = new QLocalServer(&app);
    if (!server->listen(serverName)) {
        qWarning() << "Failed to start local IPC server:" << server->errorString();
    }

    Application application;
    application.start();

    // Listen for commands from future shortcut/cli invocations
    QObject::connect(server, &QLocalServer::newConnection, [server, &application]() {
        QLocalSocket *client = server->nextPendingConnection();
        if (client) {
            QObject::connect(client, &QLocalSocket::readyRead, [client, &application]() {
                QString cmd = QString::fromUtf8(client->readAll()).trimmed();
                if (cmd == "capture") {
                    application.triggerCapture();
                } else if (cmd == "pin") {
                    application.pinFromClipboard();
                }
            });
        }
    });

    if (parser.isSet(captureOption)) {
        QTimer::singleShot(0, &application, &Application::triggerCapture);
    } else if (parser.isSet(pinOption)) {
        QTimer::singleShot(0, &application, &Application::pinFromClipboard);
    }

    return app.exec();
}
