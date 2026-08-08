#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>

#include "localsend/core.h"

#include "devicelistmodel.h"
#include "filelistmodel.h"

class AppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(int fileCount READ fileCount NOTIFY filesChanged)
  Q_PROPERTY(int onlineDeviceCount READ onlineDeviceCount NOTIFY devicesChanged)
  Q_PROPERTY(QString selfName READ selfName CONSTANT)
  Q_PROPERTY(QString selfDeviceId READ selfDeviceId CONSTANT)
  Q_PROPERTY(QString localIp READ localIp CONSTANT)
  Q_PROPERTY(bool multiSelect READ multiSelect WRITE setMultiSelect NOTIFY multiSelectChanged)
  Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
  Q_PROPERTY(int autoCollapseMs READ autoCollapseMs WRITE setAutoCollapseMs NOTIFY settingsChanged)
  Q_PROPERTY(bool floatWindowLocked READ floatWindowLocked WRITE setFloatWindowLocked NOTIFY settingsChanged)
  Q_PROPERTY(int cacheExpireHours READ cacheExpireHours WRITE setCacheExpireHours NOTIFY settingsChanged)
  Q_PROPERTY(qint64 maxRateBps READ maxRateBps WRITE setMaxRateBps NOTIFY settingsChanged)
  Q_PROPERTY(FileListModel* fileModel CONSTANT)
  Q_PROPERTY(DeviceListModel* deviceModel CONSTANT)

public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  void configure(const QString& dataDir, quint16 tcpPort, const QString& deviceName,
                 const QString& localIp);
  bool start();
  void stop();

  int fileCount() const;
  int onlineDeviceCount() const;
  QString selfName() const;
  QString selfDeviceId() const;
  QString localIp() const;

  bool multiSelect() const;
  void setMultiSelect(bool on);
  QStringList selectedIds() const;

  int autoCollapseMs() const;
  void setAutoCollapseMs(int ms);
  bool floatWindowLocked() const;
  void setFloatWindowLocked(bool locked);
  int cacheExpireHours() const;
  void setCacheExpireHours(int hours);
  qint64 maxRateBps() const;
  void setMaxRateBps(qint64 bps);

  FileListModel* fileModel() const { return fileModel_; }
  DeviceListModel* deviceModel() const { return deviceModel_; }

public slots:
  void addFiles(const QVariantList& urls);
  void toggleSelect(const QString& fileId);
  void selectAll();
  void clearSelection();
  void deleteSelected();
  void deleteFile(const QString& fileId);
  void requestPair(const QString& deviceId);
  void respondPair(const QString& deviceId, bool accept);
  void removeDevice(const QString& deviceId);
  void connectByIp(const QString& ip, int port);
  void rescan();
  void saveSettings();
  void refreshModels();

signals:
  void filesChanged();
  void devicesChanged();
  void pairRequested(const QString& deviceId, const QString& deviceName);
  void errorOccurred(const QString& message);
  void toast(const QString& message);
  void transferProgress(const QString& fileName, qreal percent);
  void settingsChanged();
  void multiSelectChanged();
  void selectionChanged();

private:
  void refreshFileModel();
  void refreshDeviceModel();

  localsend::Core& core_;
  FileListModel* fileModel_ = nullptr;
  DeviceListModel* deviceModel_ = nullptr;
  bool multiSelect_ = false;
  QSet<QString> selected_;
  std::vector<localsend::FileInfo> files_;
};
