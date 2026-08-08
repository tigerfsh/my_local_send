#include "appcontroller.h"

#include <QDebug>
#include <QMetaObject>
#include <QUrl>

#include <functional>

namespace {

void postEvent(QObject* receiver, const std::function<void()>& fn) {
  QMetaObject::invokeMethod(receiver, [fn]() { fn(); }, Qt::QueuedConnection);
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent), core_(localsend::Core::instance()) {
  fileModel_ = new FileListModel(this);
  deviceModel_ = new DeviceListModel(this);
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
  cb.onDeviceFound = [this](const localsend::Device&) {
    postEvent(this, [this]() { refreshDeviceModel(); });
  };
  cb.onDeviceOnline = [this](const localsend::Device&) {
    postEvent(this, [this]() { refreshDeviceModel(); });
  };
  cb.onDeviceOffline = [this](const localsend::Device&) {
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
    if (!url.isLocalFile()) continue;
    QString path = url.toLocalFile();
    if (core_.addFile(path.toStdString())) ++added;
  }
  refreshFileModel();
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
