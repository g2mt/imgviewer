#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>
#include <imgviewer/utils.h>

#include <QCollator>
#include <QImageReader>
#include <QPointer>
#include <QRunnable>
#include <QThreadPool>
#include <QDir>
#include <QCryptographicHash>
#include <QUrl>

#include <algorithm>

namespace {

// Returns the path to a cached thumbnail in ~/.cache/thumbnails/
// ({normal,large,x-large,xx-large}/<md5>.png). Uses XDG_CACHE_HOME or
// falls back to ~/.cache.
QString getCachedThumbnailPath(const QString &localFilePath) {
  QString fileUri = QUrl::fromLocalFile(localFilePath).toString();
  QByteArray hash =
      QCryptographicHash::hash(fileUri.toUtf8(), QCryptographicHash::Md5)
          .toHex();

  const char *dirs[] = {"normal", "large", "x-large", "xx-large"};

  const QByteArray xdgCache = qgetenv("XDG_CACHE_HOME");
  const QString cacheBase = xdgCache.isEmpty()
                                ? QDir::homePath() + QLatin1String("/.cache")
                                : QString::fromUtf8(xdgCache);
  QDir thumbDir(cacheBase + QLatin1String("/thumbnails"));

  for (const char *dir : dirs) {
    QDir sizeDir(thumbDir.filePath(QLatin1String(dir)));
    QString path = sizeDir.filePath(QString::fromLatin1(hash) + QLatin1String(".png"));
    if (QFile::exists(path))
      return path;
  }
  return {};
}

// Decodes a single image at reduced size on the global thread pool, then
// hands the result back to the model on its owning thread via a queued
// invocation. Uses QPointer so that destroying the model while jobs are
// in-flight is safe.
class ThumbnailLoader : public QRunnable {
public:
  ThumbnailLoader(ImageDetailModel *model, const QString &path, int size)
      : m_model(model), m_path(path), m_size(size) {
    setAutoDelete(true);
  }

  void run() override {
    // Try loading a pre-cached thumbnail first.
    QImage img;
    QString cachedPath = getCachedThumbnailPath(m_path);
    if (!cachedPath.isEmpty()) {
      QImageReader reader(cachedPath);
      img = reader.read();
    }

    // Fall back to decoding the original file.
    if (img.isNull()) {
      QImageReader reader(m_path);
      reader.setAutoTransform(true);
      img = reader.read();
    }

    if (!img.isNull()) {
      img = img.scaled(m_size, m_size, Qt::KeepAspectRatio,
                       Qt::SmoothTransformation);
    }
    if (m_model) {
      QMetaObject::invokeMethod(m_model, "onThumbnailReady",
                                Qt::QueuedConnection, Q_ARG(QString, m_path),
                                Q_ARG(QImage, img));
    }
  }

private:
  QPointer<ImageDetailModel> m_model;
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
  const QFileInfo &info = m_files[index.row()];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return info.fileName();
  case FilePathRole:
    return info.absoluteFilePath();
  case ThumbnailRole: {
    const QString path = info.absoluteFilePath();
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

  QDir dir = m_filter->currentPath();
  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);

  QFileInfoList entries = dir.entryInfoList();
  const QString search = m_filter->search();
  const QStringList tags = m_filter->tags();
  entries.removeIf([&](const QFileInfo &info) {
    if (!isImagePath(info.absoluteFilePath()))
      return true;
    if (!search.isEmpty() &&
        !info.fileName().contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty() && !m_filter->fileHasTags(info.absoluteFilePath()))
      return true;
    return false;
  });

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(m_filter->naturalSort());

  const SortBy sortBy = m_filter->sortBy();
  const bool descending = m_filter->descending();
  std::sort(entries.begin(), entries.end(),
            [&](const QFileInfo &a, const QFileInfo &b) {
              bool less = false;
              switch (sortBy) {
              case SortBy::Name:
                less = collator.compare(a.fileName(), b.fileName()) < 0;
                break;
              case SortBy::DateCreated:
                less = a.birthTime() < b.birthTime();
                break;
              case SortBy::DateModified:
                less = a.lastModified() < b.lastModified();
                break;
              }
              return descending ? !less : less;
            });

  m_files = entries;
  endResetModel();
}

void ImageDetailModel::requestThumbnail(const QString &path) const {
  if (m_pending.contains(path))
    return;
  m_pending.insert(path);
  auto *loader = new ThumbnailLoader(const_cast<ImageDetailModel *>(this), path,
                                     kThumbnailSize);
  QThreadPool::globalInstance()->start(loader);
}

void ImageDetailModel::onThumbnailReady(const QString &path,
                                        const QImage &image) {
  m_pending.remove(path);
  if (image.isNull())
    return;
  m_thumbnails.insert(path, QPixmap::fromImage(image));

  for (int i = 0; i < m_files.size(); ++i) {
    if (m_files[i].absoluteFilePath() == path) {
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
      break;
    }
  }
}
