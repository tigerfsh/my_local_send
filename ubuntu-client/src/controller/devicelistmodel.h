#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "localsend/types.h"

class DeviceListModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles {
    DeviceIdRole = Qt::UserRole + 1,
    DeviceNameRole,
    DeviceTypeRole,
    IsOnlineRole,
    IsTrustedRole,
    IpRole,
    TcpPortRole
  };

  explicit DeviceListModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setDevices(const std::vector<localsend::Device>& devices);

private:
  QVector<localsend::Device> devices_;
};
