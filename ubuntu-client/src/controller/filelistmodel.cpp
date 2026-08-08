#include "filelistmodel.h"

#include <QFileInfo>
#include <QUrl>

#include "localsend/types.h"

FileListModel::FileListModel(QObject* parent) : QAbstractListModel(parent) {}

int FileListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) return 0;
  return files_.size();
}

QHash<int, QByteArray> FileListModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[FileIdRole] = "fileId";
  roles[FileNameRole] = "fileName";
  roles[FileSizeRole] = "fileSize";
  roles[FileSizeTextRole] = "fileSizeText";
  roles[FileTypeRole] = "fileType";
  roles[DurationRole] = "duration";
  roles[CachePathRole] = "cachePath";
  roles[FileUrlRole] = "fileUrl";
  roles[IsImageRole] = "isImage";
  roles[IsVideoRole] = "isVideo";
  return roles;
}

QVariant FileListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= files_.size()) return {};
  const localsend::FileInfo& f = files_.at(index.row());
  switch (role) {
    case FileIdRole: return QString::fromStdString(f.fileId);
    case FileNameRole: return QString::fromStdString(f.fileName);
    case FileSizeRole: return static_cast<qulonglong>(f.fileSize);
    case FileSizeTextRole: return QString::fromStdString(localsend::formatFileSize(f.fileSize));
    case FileTypeRole: return QString::fromStdString(localsend::fileTypeToString(f.fileType));
    case DurationRole: return static_cast<qlonglong>(f.duration);
    case CachePathRole: return QString::fromStdString(f.cachePath);
    case FileUrlRole: {
      const QString path = QString::fromStdString(f.cachePath);
      return path.isEmpty() ? QVariant(QUrl()) : QVariant(QUrl::fromLocalFile(path));
    }
    case IsImageRole: return f.fileType == localsend::FileType::Image;
    case IsVideoRole: return f.fileType == localsend::FileType::Video;
    default: return {};
  }
}

void FileListModel::setFiles(const std::vector<localsend::FileInfo>& files) {
  beginResetModel();
  files_.clear();
  for (const auto& f : files) files_.push_back(f);
  endResetModel();
}

std::vector<localsend::FileInfo> FileListModel::files() const {
  std::vector<localsend::FileInfo> out;
  out.reserve(static_cast<size_t>(files_.size()));
  for (const auto& f : files_) out.push_back(f);
  return out;
}

bool FileListModel::contains(const QString& fileId) const {
  for (const auto& f : files_) {
    if (QString::fromStdString(f.fileId) == fileId) return true;
  }
  return false;
}
