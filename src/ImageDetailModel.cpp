#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>

#include <QBuffer>
#include <QCollator>
#include <QImageReader>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <algorithm>

namespace {

constexpr int kRemoteThumbnailSize = ImageDetailModel::kThumbnailSize;

class ThumbnailLoader : public QRunnable {
public:
  ThumbnailLoader(ImageDetailModel *model, Filter *filter, const QString &path,
                  int size)
      : m_model(model), m_filter(filter), m_path(path), m_size(size) {
    setAutoDelete(true);
  }

  void run() override {
    QImage image;
    const QByteArray bytes = m_filter->readFileBytes(m_path);
    if (!bytes.isEmpty()) {
      QBuffer buffer;
      buffer.setData(bytes);
      buffer.open(QIODevice::ReadOnly);
      QImageReader reader(&buffer);
      reader.setAutoTransform(true);
      image = reader.read();
    }

    if (!image.isNull()) {
      image = image.scaled(m_size, m_size, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    }
    if (m_model) {
      QMetaObject::invokeMethod(m_model, "onThumbnailReady",
                                Qt::QueuedConnection, Q_ARG(QString, m_path),
                                Q_ARG(QImage, image));
    }
  }

private:
  QPointer<ImageDetailModel> m_model;
  Filter *m_filter;
  QString m_path;
  int m_size;
};

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
    return entry.name;
  case FilePathRole:
    return entry.path;
  case ThumbnailRole: {
    const QString path = entry.path;
    auto it = m_thumbnails.constFind(path);
    if (it != m_thumbnails.constEnd())
      return *it;
    requestThumbnail(path);
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

  auto entries = m_filter->listDirectoryEntries(m_filter->currentPath());
  const QString search = m_filter->search();
  const QList<QString> tags = m_filter->tags();
  entries.removeIf([&](const DirectoryEntry &entry) {
    if (entry.isDir)
      return true;
    if (!m_filter->isImagePath(entry.path))
      return true;
    if (!search.isEmpty() && !entry.name.contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty() && !m_filter->fileHasTags(entry.path))
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
                less = collator.compare(a.name, b.name) < 0;
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

void ImageDetailModel::requestThumbnail(const QString &path) const {
  if (m_pending.contains(path))
    return;
  m_pending.insert(path);
  auto *loader = new ThumbnailLoader(const_cast<ImageDetailModel *>(this),
                                     m_filter, path, kRemoteThumbnailSize);
  QThreadPool::globalInstance()->start(loader);
}

void ImageDetailModel::onThumbnailReady(const QString &path,
                                        const QImage &image) {
  m_pending.remove(path);
  if (image.isNull())
    return;
  m_thumbnails.insert(path, QPixmap::fromImage(image));

  for (int i = 0; i < m_files.size(); ++i) {
    if (m_files[i].path == path) {
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
      break;
    }
  }
}
