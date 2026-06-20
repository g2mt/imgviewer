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
  const auto &entry = m_files[index.row()];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return entry->name();
  case FilePathRole: {
    if (auto *pdfEntry = qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return QVariant::fromValue(pdfEntry);
    return qobject_cast<DirectoryEntry *>(entry.data())->url().toString();
  }
  case ThumbnailRole: {
    if (auto *pdfEntry = qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return QPixmap::fromImage(pdfEntry->renderPage());
    const QString key =
        qobject_cast<DirectoryEntry *>(entry.data())->url().toString();
    auto it = m_thumbnails.constFind(key);
    if (it != m_thumbnails.constEnd())
      return *it;
    if (qobject_cast<DirectoryEntry *>(entry.data()))
      requestThumbnail(qSharedPointerCast<DirectoryEntry>(entry));
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
  entries.removeIf([&](const QSharedPointer<BaseDirectoryEntry> &entry) {
    if (qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return false;
    if (entry->isDir())
      return true;
    if (!entry->isImagePath())
      return true;
    if (!search.isEmpty() &&
        !entry->name().contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty()) {
      auto *de = qobject_cast<DirectoryEntry *>(entry.data());
      if (de && !m_filter->fileHasTags(de->url().toString()))
        return true;
    }
    return false;
  });

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(m_filter->naturalSort());

  const SortBy sortBy = m_filter->sortBy();
  const bool descending = m_filter->descending();
  std::sort(entries.begin(), entries.end(),
            [&](const QSharedPointer<BaseDirectoryEntry> &a,
                const QSharedPointer<BaseDirectoryEntry> &b) {
              bool less = false;
              switch (sortBy) {
              case SortBy::Name:
                less = collator.compare(a->name(), b->name()) < 0;
                break;
              case SortBy::DateCreated:
                less = a->birthTime() < b->birthTime();
                break;
              case SortBy::DateModified:
                less = a->lastModified() < b->lastModified();
                break;
              }
              return descending ? !less : less;
            });

  m_files = std::move(entries);
  endResetModel();
}

void ImageDetailModel::requestThumbnail(
    const QSharedPointer<DirectoryEntry> &entry) const {
  const QString path = entry->url().toString();
  if (m_pending.contains(path))
    return;
  m_pending.insert(path);

  auto *self = const_cast<ImageDetailModel *>(this);

#if defined(USE_LIBARCHIVE)
  QImage image;
  if (entry->url().isLocalFile()) {
    QImageReader reader(entry->url().toLocalFile());
    reader.setAutoTransform(true);
    image = reader.read();
  }
  if (!image.isNull()) {
    image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  }
  self->onThumbnailReady(path, image);
#elif defined(USE_KIO)
  KIO::StoredTransferJob *job =
      KIO::storedGet(entry->url(), KIO::NoReload);
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
    if (auto *de = qobject_cast<DirectoryEntry *>(m_files[i].data())) {
      if (de->url().toString() == path) {
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
        break;
      }
    }
  }
}
