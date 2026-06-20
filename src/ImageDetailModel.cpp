#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>

#ifdef USE_KIO
#include <KIO/StoredTransferJob>
#endif
#include <QBuffer>
#include <QCollator>
#include <QImageReader>
#include <algorithm>

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
    return entry.name();
  case FilePathRole: {
    if (auto *ve = std::get_if<VirtualDirectoryEntry>(&entry.path))
      return ve->image;
    return std::get<QUrl>(entry.path).toString();
  }
  case ThumbnailRole: {
    if (auto *ve = std::get_if<VirtualDirectoryEntry>(&entry.path))
      return QPixmap::fromImage(ve->image);
    const QString key = std::get<QUrl>(entry.path).toString();
    auto it = m_thumbnails.constFind(key);
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
    if (std::holds_alternative<VirtualDirectoryEntry>(entry.path))
      return false;
    if (entry.isDir)
      return true;
    if (!entry.isImagePath())
      return true;
    if (!search.isEmpty() &&
        !entry.name().contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty() &&
        !m_filter->fileHasTags(std::get<QUrl>(entry.path).toString()))
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
                less = collator.compare(a.name(), b.name()) < 0;
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
  const QString path = std::get<QUrl>(entry.path).toString();
  if (m_pending.contains(path))
    return;
  m_pending.insert(path);

  auto *self = const_cast<ImageDetailModel *>(this);

#if defined(USE_LIBARCHIVE)
  QImage image;
  if (auto *url = std::get_if<QUrl>(&entry.path)) {
    if (url->isLocalFile()) {
      QImageReader reader(url->toLocalFile());
      reader.setAutoTransform(true);
      image = reader.read();
    }
  }
  if (!image.isNull()) {
    image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  }
  self->onThumbnailReady(path, image);
#elif defined(USE_KIO)
  KIO::StoredTransferJob *job =
      KIO::storedGet(std::get<QUrl>(entry.path), KIO::NoReload);
  connect(job, &KIO::StoredTransferJob::result, self, [self, job, path]() {
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
      image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    }
    self->onThumbnailReady(path, image);
  });
#endif
}

void ImageDetailModel::onThumbnailReady(const QString &path,
                                        const QImage &image) {
  m_pending.remove(path);
  if (image.isNull())
    return;
  m_thumbnails.insert(path, QPixmap::fromImage(image));

  for (int i = 0; i < m_files.size(); ++i) {
    if (std::get<QUrl>(m_files[i].path).toString() == path) {
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
      break;
    }
  }
}
