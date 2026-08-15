#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include "localsend/core.h"

#include "devicelistmodel.h"
#include "filelistmodel.h"

class QQuickWindow;
class QMediaPlayer;
class QVideoSink;
class QVideoFrame;

class AppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(int fileCount READ fileCount NOTIFY filesChanged)
  Q_PROPERTY(int onlineDeviceCount READ onlineDeviceCount NOTIFY devicesChanged)
  Q_PROPERTY(QString selfName READ selfName CONSTANT)
  Q_PROPERTY(QString selfDeviceId READ selfDeviceId CONSTANT)
  Q_PROPERTY(QString localIp READ localIp CONSTANT)
  Q_PROPERTY(int windowY READ windowY CONSTANT)
  Q_PROPERTY(bool multiSelect READ multiSelect WRITE setMultiSelect NOTIFY multiSelectChanged)
  Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
  Q_PROPERTY(int autoCollapseMs READ autoCollapseMs WRITE setAutoCollapseMs NOTIFY settingsChanged)
  Q_PROPERTY(bool pinned READ pinned WRITE setPinned NOTIFY pinnedChanged)
  Q_PROPERTY(bool floatWindowLocked READ floatWindowLocked WRITE setFloatWindowLocked NOTIFY settingsChanged)
  Q_PROPERTY(int cacheExpireHours READ cacheExpireHours WRITE setCacheExpireHours NOTIFY settingsChanged)
  Q_PROPERTY(qint64 maxRateBps READ maxRateBps WRITE setMaxRateBps NOTIFY settingsChanged)
  Q_PROPERTY(FileListModel* fileModel READ fileModel CONSTANT)
  Q_PROPERTY(DeviceListModel* deviceModel READ deviceModel CONSTANT)

public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  void configure(const QString& dataDir, quint16 tcpPort, const QString& deviceName,
                 const QString& localIp);
  void setWindowY(int y) { windowY_ = y; }
  int windowY() const { return windowY_; }
  void setWindow(QQuickWindow* w) { window_ = w; }
  Q_INVOKABLE void setWindowMask(int x, int y, int w, int h);
  Q_INVOKABLE void clearWindowMask();
  // Returns a file:// URL to a thumbnail for the given cached file:
  // image -> itself, video -> extracted first frame, doc -> system file icon.
  Q_INVOKABLE QString thumbUrl(const QString& fileId, const QString& cachePath, const QString& fileType);
  Q_INVOKABLE QString cacheDir() const;
  Q_INVOKABLE bool setCacheDir(const QString& dir);
  // Reads a small text file for in-app preview (empty if binary/unreadable).
  Q_INVOKABLE QString readTextFile(const QString& path);
  // Opens a file with the system's default application.
  Q_INVOKABLE void openWithSystem(const QString& path);
  // Builds the "text/uri-list" payload for dragging files out: in multi-select
  // mode it returns all selected files, otherwise the single file being dragged.
  Q_INVOKABLE QString dragUriList(const QString& fileId) const;
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
  bool pinned() const;
  void setPinned(bool on);
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
  void thumbnailReady(const QString& fileId);
  void pinnedChanged();

private:
  void refreshFileModel();
  void refreshDeviceModel();
  void processNextVideoThumb();
  void onVideoFrame(const QVideoFrame& frame);

  localsend::Core& core_;
  FileListModel* fileModel_ = nullptr;
  DeviceListModel* deviceModel_ = nullptr;
  bool multiSelect_ = false;
  int windowY_ = 90;
  QQuickWindow* window_ = nullptr;
  QSet<QString> selected_;
  std::vector<localsend::FileInfo> files_;

  QHash<QString, QString> thumbCache_;   // fileId -> thumbnail file path
  QMediaPlayer* videoPlayer_ = nullptr;
  QVideoSink* videoSink_ = nullptr;
  QStringList videoQueue_;               // pairs: fileId, videoPath, ...
  QString currentVideoFileId_;
  QString currentVideoThumbPath_;
};
