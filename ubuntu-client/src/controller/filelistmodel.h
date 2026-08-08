#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "localsend/types.h"

class FileListModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles {
    FileIdRole = Qt::UserRole + 1,
    FileNameRole,
    FileSizeRole,
    FileSizeTextRole,
    FileTypeRole,
    DurationRole,
    CachePathRole,
    FileUrlRole,
    IsImageRole,
    IsVideoRole
  };

  explicit FileListModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setFiles(const std::vector<localsend::FileInfo>& files);
  std::vector<localsend::FileInfo> files() const;
  bool contains(const QString& fileId) const;

private:
  QVector<localsend::FileInfo> files_;
};
