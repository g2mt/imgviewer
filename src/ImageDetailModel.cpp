#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>

#include <KIO/StoredTransferJob>
#include <QBuffer>
#include <QCollator>
#include <QImageReader>
#include <algorithm>

namespace {

constexpr int kRemoteThumbnailSize = ImageDetailModel::kThumbnailSize;

} // namespace

ImageDetailModel::ImageDetailModel(Filter *filter, QObject *parent)
    : QAbstractListModel(parent), m_filter(filter) {
  reload();
  connect(m_filter, &Filter::changed, this, &ImageDetailModel::reload);
}

int ImageDetailModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_files.size();
}

QVariant ImageDetailModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size())
    return {};
  const DirectoryEntry &entry = m_files[index.row()];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return entry.path.fileName();
  case FilePathRole:
    return entry.path.toString();
  case ThumbnailRole: {
    const QString path = entry.path.toString();
    auto it = m_thumbnails.constFind(path);
    if (it != m_thumbnails.constEnd())
      return *it;
    requestThumbnail(entry);
    return {};
  }
  default:
    return {};
  }
}

void ImageDetailModel::reload() {
  beginResetModel();
  m_files.clear();
  m_thumbnails.clear();
  m_pending.clear();

  auto entries = m_filter->listDirectoryEntries();
  const QString search = m_filter->search();
  const QList<QString> tags = m_filter->tags();
  entries.removeIf([&](const DirectoryEntry &entry) {
    if (entry.isDir)
      return true;
    if (!entry.isImagePath())
      return true;
    if (!search.isEmpty() && !entry.path.fileName().contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty() && !m_filter->fileHasTags(entry.path.toString()))
      return true;
    return false;
  });

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(m_filter->naturalSort());

  const SortBy sortBy = m_filter->sortBy();
  const bool descending = m_filter->descending();
  std::sort(entries.begin(), entries.end(),
            [&](const DirectoryEntry &a, const DirectoryEntry &b) {
              bool less = false;
              switch (sortBy) {
              case SortBy::Name:
                less = collator.compare(a.path.fileName(), b.path.fileName()) < 0;
                break;
              case SortBy::DateCreated:
                less = a.birthTime < b.birthTime;
                break;
              case SortBy::DateModified:
                less = a.lastModified < b.lastModified;
                break;
              }
              return descending ? !less : less;
            });

  m_files = std::move(entries);
  endResetModel();
}

void ImageDetailModel::requestThumbnail(const DirectoryEntry &entry) const {
  const QString path = entry.path.toString();
  if (m_pending.contains(path))
    return;
  m_pending.insert(path);

  auto *self = const_cast<ImageDetailModel *>(this);
  KIO::StoredTransferJob *job = KIO::storedGet(entry.path, KIO::NoReload);
  connect(job, &KIO::StoredTransferJob::result, self,
          [self, job, path]() {
            if (!self->m_pending.contains(path))
              return;

            QImage image;
            if (!job->error()) {
              const QByteArray bytes = job->data();
              QBuffer buffer;
              buffer.setData(bytes);
              buffer.open(QIODevice::ReadOnly);
              QImageReader reader(&buffer);
              reader.setAutoTransform(true);
              image = reader.read();
            }

            if (!image.isNull()) {
              image = image.scaled(kRemoteThumbnailSize, kRemoteThumbnailSize,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
            }
            self->onThumbnailReady(path, image);
          });
}

void ImageDetailModel::onThumbnailReady(const QString &path,
                                        const QImage &image) {
  m_pending.remove(path);
  if (image.isNull())
    return;
  m_thumbnails.insert(path, QPixmap::fromImage(image));

  for (int i = 0; i < m_files.size(); ++i) {
    if (m_files[i].path.toString() == path) {
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
      break;
    }
  }
}
