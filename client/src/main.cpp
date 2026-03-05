#include "backend/networkclient.h"
#include <QDebug>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtGlobal>

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
  QGuiApplication app(argc, argv);

  // Register Fonts
  QStringList fontPaths = {
      ":/fonts/Pretendard-Thin.otf",   ":/fonts/Pretendard-ExtraLight.otf",
      ":/fonts/Pretendard-Light.otf",  ":/fonts/Pretendard-Regular.otf",
      ":/fonts/Pretendard-Medium.otf", ":/fonts/Pretendard-SemiBold.otf",
      ":/fonts/Pretendard-Bold.otf",   ":/fonts/Pretendard-ExtraBold.otf",
      ":/fonts/Pretendard-Black.otf"};

  for (const QString &path : fontPaths) {
    if (QFontDatabase::addApplicationFont(path) == -1) {
      qWarning() << "Failed to load font:" << path;
    }
  }

  app.setFont(QFont("Pretendard"));

  // Register Network Client
  qmlRegisterType<NetworkClient>("com.metro.network", 1, 0, "NetworkClient");

  QQmlApplicationEngine engine;

  g_cameraImageProvider = new CameraImageProvider();
  engine.addImageProvider("camera", g_cameraImageProvider);

  const QUrl url(QStringLiteral("qrc:/Main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
