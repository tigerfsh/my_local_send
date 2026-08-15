#include "appcontroller.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QPixmap>
#include <QQuickWindow>
#include <QRegion>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <functional>

namespace {

void postEvent(QObject* receiver, const std::function<void()>& fn) {
  QMetaObject::invokeMethod(receiver, [fn]() { fn(); }, Qt::QueuedConnection);
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent), core_(localsend::Core::instance()) {
  fileModel_ = new FileListModel(this);
  deviceModel_ = new DeviceListModel(this);
  connect(this, &AppController::thumbnailReady, this, [this](const QString&) {
    refreshFileModel();
  });
}

AppController::~AppController() {
  core_.stop();
}

void AppController::configure(const QString& dataDir, quint16 tcpPort, const QString& deviceName,
                              const QString& localIp) {
  localsend::CoreConfig cfg;
  cfg.appName = "中转站";
  cfg.deviceName = deviceName.toStdString();
  cfg.deviceType = localsend::DeviceType::Ubuntu;
  cfg.dataDir = dataDir.toStdString();
  cfg.tcpPort = tcpPort;
  core_.configure(cfg);
  if (!localIp.isEmpty()) core_.setLocalIp(localIp.toStdString());

  localsend::Callbacks cb;
  cb.onFileAdded = [this](const localsend::FileInfo&) {
    postEvent(this, [this]() { refreshFileModel(); });
  };
  cb.onFileRemoved = [this](const localsend::FileInfo&) {
    postEvent(this, [this]() { refreshFileModel(); });
  };
  cb.onDeviceFound = [this](const localsend::Device& d) {
    qDebug().noquote() << "[discovery] found" << QString::fromStdString(d.deviceId)
                       << QString::fromStdString(d.deviceName)
                       << QString::fromStdString(d.ip) << d.tcpPort
                       << "trusted=" << d.isTrusted;
    postEvent(this, [this]() { refreshDeviceModel(); });
  };
  cb.onDeviceOnline = [this](const localsend::Device& d) {
    qDebug().noquote() << "[discovery] online" << QString::fromStdString(d.deviceName);
    postEvent(this, [this]() { refreshDeviceModel(); });
  };
  cb.onDeviceOffline = [this](const localsend::Device& d) {
    qDebug().noquote() << "[discovery] offline" << QString::fromStdString(d.deviceName);
    postEvent(this, [this]() { refreshDeviceModel(); });
  };
  cb.onPairRequest = [this](const localsend::Device& d) {
    QString id = QString::fromStdString(d.deviceId);
    QString name = QString::fromStdString(d.deviceName);
    postEvent(this, [this, id, name]() { emit pairRequested(id, name); });
  };
  cb.onPairResult = [this](const localsend::Device&, bool accepted) {
    postEvent(this, [this, accepted]() {
      refreshDeviceModel();
      emit toast(accepted ? "配对成功" : "配对已拒绝");
    });
  };
  cb.onTransferFinished = [this](const std::string&, const std::string& fileName, bool ok) {
    QString name = QString::fromStdString(fileName);
    postEvent(this, [this, name, ok]() {
      emit toast(ok ? QString("已同步: %1").arg(name) : QString("同步失败: %1").arg(name));
    });
  };
  cb.onTransferProgress = [this](const localsend::TransferProgress& p) {
    QString name = QString::fromStdString(p.fileName);
    qreal percent = p.percent;
    postEvent(this, [this, name, percent]() { emit transferProgress(name, percent); });
  };
  cb.onError = [this](const std::string& message) {
    QString msg = QString::fromStdString(message);
    postEvent(this, [this, msg]() { emit errorOccurred(msg); });
  };
  core_.setCallbacks(cb);
}

bool AppController::start() {
  bool ok = core_.start();
  if (ok) refreshModels();
  return ok;
}

void AppController::stop() { core_.stop(); }

void AppController::setWindowMask(int x, int y, int w, int h) {
  if (window_) window_->setMask(QRegion(x, y, w, h));
}

void AppController::clearWindowMask() {
  if (window_) window_->setMask(QRegion());
}

QString AppController::cacheDir() const {
  return QString::fromStdString(core_.cacheDir());
}

bool AppController::setCacheDir(const QString& dir) {
  QString path = dir;
  const QUrl url(dir);
  if (url.isLocalFile()) path = url.toLocalFile();  // handle file:// from FolderDialog
  return core_.setCacheDir(path.toStdString());
}

QString AppController::readTextFile(const QString& path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return QString();
  QByteArray data = f.read(256 * 1024);  // cap preview at 256KB
  f.close();
  if (data.isEmpty() || data.contains('\0')) return QString();  // empty/binary
  QString text = QString::fromUtf8(data);
  if (text.contains(QChar(0xFFFD))) text = QString::fromLocal8Bit(data);
  return text;
}

void AppController::openWithSystem(const QString& path) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString AppController::dragUriList(const QString& fileId) const {
  QStringList urls;
  if (multiSelect_) {
    for (const auto& f : files_) {
      const QString id = QString::fromStdString(f.fileId);
      if (selected_.contains(id)) {
        urls << QUrl::fromLocalFile(QString::fromStdString(f.cachePath)).toString();
      }
    }
  }
  if (urls.isEmpty()) {
    for (const auto& f : files_) {
      if (QString::fromStdString(f.fileId) == fileId) {
        urls << QUrl::fromLocalFile(QString::fromStdString(f.cachePath)).toString();
        break;
      }
    }
  }
  return urls.join(QLatin1Char('\n'));
}

QString AppController::thumbUrl(const QString& fileId, const QString& cachePath, const QString& fileType) {
  if (fileType == "image") {
    return QUrl::fromLocalFile(cachePath).toString();
  }
  // Already generated (or generating) thumbnail.
  if (thumbCache_.contains(fileId)) {
    const QString p = thumbCache_.value(fileId);
    if (QFileInfo::exists(p)) return QUrl::fromLocalFile(p).toString();
    thumbCache_.remove(fileId);
  }
  const QString thumbPath = cachePath + ".thumb.png";
  if (QFileInfo::exists(thumbPath)) {
    thumbCache_[fileId] = thumbPath;
    return QUrl::fromLocalFile(thumbPath).toString();
  }
  if (fileType == "video") {
    // First-frame extraction is async; QML shows the "▶" fallback until ready.
    videoQueue_ << fileId << cachePath;
    processNextVideoThumb();
    return QString();
  }
  // doc / other / unknown: generic document icon from the icon theme.
  QIcon icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
  if (icon.isNull()) icon = QIcon::fromTheme(QStringLiteral("application-x-generic"));
  QPixmap pm = icon.pixmap(96, 96);
  if (!pm.isNull() && pm.save(thumbPath, "PNG")) {
    thumbCache_[fileId] = thumbPath;
    return QUrl::fromLocalFile(thumbPath).toString();
  }
  return QString();
}

void AppController::processNextVideoThumb() {
  if (!currentVideoFileId_.isEmpty() || videoQueue_.size() < 2) return;
  const QString fileId = videoQueue_.takeFirst();
  const QString path = videoQueue_.takeFirst();
  if (!videoPlayer_) {
    videoPlayer_ = new QMediaPlayer(this);
    videoSink_ = new QVideoSink(this);
    videoPlayer_->setVideoSink(videoSink_);
    connect(videoSink_, &QVideoSink::videoFrameChanged, this, &AppController::onVideoFrame);
    connect(videoPlayer_, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString&) {
              if (currentVideoFileId_.isEmpty()) return;
              videoPlayer_->stop();
              currentVideoFileId_.clear();
              currentVideoThumbPath_.clear();
              processNextVideoThumb();
            });
  }
  currentVideoFileId_ = fileId;
  currentVideoThumbPath_ = path + ".thumb.png";
  videoPlayer_->setSource(QUrl::fromLocalFile(path));
  videoPlayer_->play();
  // Give up if no frame arrives (e.g. missing decoder), so the next video runs.
  QTimer::singleShot(5000, this, [this, fileId]() {
    if (currentVideoFileId_ == fileId) {
      videoPlayer_->stop();
      currentVideoFileId_.clear();
      currentVideoThumbPath_.clear();
      processNextVideoThumb();
    }
  });
}

void AppController::onVideoFrame(const QVideoFrame& frame) {
  if (currentVideoFileId_.isEmpty()) return;
  QVideoFrame f = frame;
  if (f.isValid() && f.map(QVideoFrame::ReadOnly)) {
    QImage img = f.toImage();
    f.unmap();
    if (!img.isNull()) {
      img.save(currentVideoThumbPath_, "PNG");
      thumbCache_[currentVideoFileId_] = currentVideoThumbPath_;
      emit thumbnailReady(currentVideoFileId_);
    }
  }
  videoPlayer_->stop();
  currentVideoFileId_.clear();
  currentVideoThumbPath_.clear();
  processNextVideoThumb();
}

int AppController::fileCount() const { return static_cast<int>(files_.size()); }

int AppController::onlineDeviceCount() const {
  int n = 0;
  for (const auto& d : core_.listDevices()) {
    if (d.isOnline) ++n;
  }
  return n;
}

QString AppController::selfName() const { return QString::fromStdString(core_.deviceName()); }

QString AppController::selfDeviceId() const { return QString::fromStdString(core_.deviceId()); }

QString AppController::localIp() const { return QString::fromStdString(core_.localIp()); }

bool AppController::multiSelect() const { return multiSelect_; }

void AppController::setMultiSelect(bool on) {
  if (multiSelect_ == on) return;
  multiSelect_ = on;
  if (!on) {
    selected_.clear();
    emit selectionChanged();
  }
  emit multiSelectChanged();
}

QStringList AppController::selectedIds() const {
  QStringList list;
  for (const auto& id : selected_) list << id;
  return list;
}

int AppController::autoCollapseMs() const { return core_.settings().autoCollapseMs; }

void AppController::setAutoCollapseMs(int ms) {
  if (core_.settings().autoCollapseMs == ms) return;
  core_.settings().autoCollapseMs = ms;
  core_.saveSettings();
  emit settingsChanged();
}

bool AppController::pinned() const { return core_.settings().pinned; }

void AppController::setPinned(bool on) {
  if (core_.settings().pinned == on) return;
  core_.settings().pinned = on;
  core_.saveSettings();
  emit pinnedChanged();
}

bool AppController::floatWindowLocked() const { return core_.settings().floatWindowLocked; }

void AppController::setFloatWindowLocked(bool locked) {
  if (core_.settings().floatWindowLocked == locked) return;
  core_.settings().floatWindowLocked = locked;
  core_.saveSettings();
  emit settingsChanged();
}

int AppController::cacheExpireHours() const { return core_.settings().cacheExpireHours; }

void AppController::setCacheExpireHours(int hours) {
  if (core_.settings().cacheExpireHours == hours) return;
  core_.settings().cacheExpireHours = hours;
  core_.saveSettings();
  emit settingsChanged();
}

qint64 AppController::maxRateBps() const { return core_.settings().maxTransferRateBps; }

void AppController::setMaxRateBps(qint64 bps) {
  if (core_.settings().maxTransferRateBps == bps) return;
  core_.settings().maxTransferRateBps = bps;
  core_.saveSettings();
  emit settingsChanged();
}

void AppController::addFiles(const QVariantList& urls) {
  int added = 0;
  for (const QVariant& v : urls) {
    QUrl url = v.toUrl();
    qDebug().noquote() << "[addFiles] url=" << url.toString()
                       << "isLocalFile=" << url.isLocalFile();
    if (!url.isLocalFile()) continue;
    QString path = url.toLocalFile();
    qDebug().noquote() << "[addFiles] path=" << path;
    if (core_.addFile(path.toStdString())) {
      ++added;
    } else {
      qDebug().noquote() << "[addFiles] addFile failed for" << path;
    }
  }
  refreshFileModel();
  qDebug().noquote() << "[addFiles] added=" << added << "total=" << files_.size();
  if (added > 0) emit toast(QString("已添加 %1 个文件").arg(added));
}

void AppController::toggleSelect(const QString& fileId) {
  if (selected_.contains(fileId)) selected_.remove(fileId);
  else selected_.insert(fileId);
  emit selectionChanged();
}

void AppController::selectAll() {
  for (const auto& f : files_) selected_.insert(QString::fromStdString(f.fileId));
  emit selectionChanged();
}

void AppController::clearSelection() {
  selected_.clear();
  emit selectionChanged();
}

void AppController::deleteSelected() {
  int removed = 0;
  for (const auto& id : selected_) {
    if (core_.removeFile(id.toStdString())) ++removed;
  }
  selected_.clear();
  emit selectionChanged();
  refreshFileModel();
  if (removed > 0) emit toast(QString("已删除 %1 个文件").arg(removed));
}

void AppController::deleteFile(const QString& fileId) {
  if (core_.removeFile(fileId.toStdString())) {
    selected_.remove(fileId);
    emit selectionChanged();
    refreshFileModel();
    emit toast("文件已删除");
  }
}

void AppController::requestPair(const QString& deviceId) {
  core_.requestPair(deviceId.toStdString());
}

void AppController::respondPair(const QString& deviceId, bool accept) {
  core_.pairDevice(deviceId.toStdString(), accept);
}

void AppController::removeDevice(const QString& deviceId) {
  core_.removeDevice(deviceId.toStdString());
  refreshDeviceModel();
}

void AppController::connectByIp(const QString& ip, int port) {
  if (core_.connectDeviceByIp(ip.toStdString(), static_cast<uint16_t>(port))) {
    refreshDeviceModel();
    emit toast(QString("已连接 %1:%2，可在设备列表中发起配对").arg(ip).arg(port));
  } else {
    emit errorOccurred("手动连接失败");
  }
}

void AppController::rescan() {
  core_.rescan();
}

void AppController::saveSettings() {
  core_.saveSettings();
  emit settingsChanged();
}

void AppController::refreshModels() {
  refreshFileModel();
  refreshDeviceModel();
}

void AppController::refreshFileModel() {
  files_ = core_.listFiles();
  // Drop selections for files that no longer exist.
  QStringList stale;
  for (const auto& id : selected_) {
    bool found = false;
    for (const auto& f : files_) {
      if (QString::fromStdString(f.fileId) == id) { found = true; break; }
    }
    if (!found) stale << id;
  }
  for (const auto& id : stale) selected_.remove(id);
  if (!stale.isEmpty()) emit selectionChanged();
  fileModel_->setFiles(files_);
  emit filesChanged();
}

void AppController::refreshDeviceModel() {
  deviceModel_->setDevices(core_.listDevices());
  emit devicesChanged();
}
