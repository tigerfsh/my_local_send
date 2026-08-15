#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSysInfo>

#include "appcontroller.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName("中转站");
  app.setOrganizationName("localsend");

  QString dataDir = QDir::home().filePath(".localsend");
  int tcpPort = 53318;
  QString deviceName = QSysInfo::machineHostName();
  QString localIp;
  int windowY = 90;

  const QStringList args = QCoreApplication::arguments();
  for (int i = 1; i < args.size(); ++i) {
    if (args[i] == "--data-dir" && i + 1 < args.size()) dataDir = args[++i];
    else if (args[i] == "--tcp-port" && i + 1 < args.size()) tcpPort = args[++i].toInt();
    else if (args[i] == "--device-name" && i + 1 < args.size()) deviceName = args[++i];
    else if (args[i] == "--local-ip" && i + 1 < args.size()) localIp = args[++i];
    else if (args[i] == "--window-y" && i + 1 < args.size()) windowY = args[++i].toInt();
  }

  AppController controller;
  controller.configure(dataDir, static_cast<quint16>(tcpPort), deviceName, localIp);
  controller.setWindowY(windowY);
  controller.start();

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("app", &controller);
  engine.rootContext()->setContextProperty("fileModel", controller.fileModel());
  engine.rootContext()->setContextProperty("deviceModel", controller.deviceModel());
  engine.load(QUrl(QStringLiteral("qrc:/LocalSend/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) return -1;

  if (auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0))) {
    controller.setWindow(win);
  }

  return app.exec();
}
