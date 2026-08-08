#include "devicelistmodel.h"

DeviceListModel::DeviceListModel(QObject* parent) : QAbstractListModel(parent) {}

int DeviceListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return devices_.size();
}

QHash<int, QByteArray> DeviceListModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[DeviceIdRole] = "deviceId";
  roles[DeviceNameRole] = "deviceName";
  roles[DeviceTypeRole] = "deviceType";
  roles[IsOnlineRole] = "isOnline";
  roles[IsTrustedRole] = "isTrusted";
  roles[IpRole] = "ip";
  roles[TcpPortRole] = "tcpPort";
  return roles;
}

QVariant DeviceListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= devices_.size()) return {};
  const localsend::Device& d = devices_.at(index.row());
  switch (role) {
    case DeviceIdRole: return QString::fromStdString(d.deviceId);
    case DeviceNameRole: return QString::fromStdString(d.deviceName);
    case DeviceTypeRole: return QString::fromStdString(localsend::deviceTypeToString(d.deviceType));
    case IsOnlineRole: return d.isOnline;
    case IsTrustedRole: return d.isTrusted;
    case IpRole: return QString::fromStdString(d.ip);
    case TcpPortRole: return static_cast<int>(d.tcpPort);
    default: return {};
  }
}

void DeviceListModel::setDevices(const std::vector<localsend::Device>& devices) {
  beginResetModel();
  devices_.clear();
  for (const auto& d : devices) devices_.push_back(d);
  endResetModel();
}
