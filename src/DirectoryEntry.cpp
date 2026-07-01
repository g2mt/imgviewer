#include <imgviewer/DirectoryEntry.h>

#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#ifdef USE_KIO
#include <KIO/ListJob>
#include <KIO/StoredTransferJob>
#endif

void BaseDirectoryEntry::requestThumbnail() {}

BaseDirectoryEntry::EntryType BaseDirectoryEntry::entryType() const {
  return EntryType::Dir;
}

static bool isImageSuffix(const QString &ext) {
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

static bool isArchiveSuffix(const QString &ext) {
#ifdef USE_QT_PDF
  if (ext == "pdf")
    return true;
#endif
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}

/** DirectoryEntry factory methods **/

DirectoryEntry *DirectoryEntry::fromFileInfo(const QFileInfo &info,
                                             QObject *parent) {
  if (info.fileName() == QLatin1String("."))
    return nullptr;

  const QString ext = info.suffix().toLower();
  EntryType type;
  if (info.isDir())
    type = EntryType::Dir;
  else if (isImageSuffix(ext))
    type = EntryType::Image;
  else if (isArchiveSuffix(ext))
    type = EntryType::Archive;
  else
    return nullptr;

  auto *entry = new DirectoryEntry(parent);
  entry->m_url = QUrl::fromLocalFile(info.absoluteFilePath());
  entry->m_name = info.fileName();
  entry->m_entryType = type;
  entry->m_birthTime = info.birthTime();
  entry->m_lastModified = info.lastModified();
  return entry;
}

#ifdef USE_KIO
DirectoryEntry *DirectoryEntry::fromKio(const KIO::UDSEntry &uds,
                                        const QUrl &parentDir,
                                        QObject *parent) {
  const QString name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
  if (name.isEmpty() || name == QLatin1String(".") ||
      name == QLatin1String(".."))
    return nullptr;

  auto *entry = new DirectoryEntry(parent);
  entry->m_url = parentDir.resolved(name);
  entry->m_name = name;
  entry->m_entryType = S_ISDIR(uds.numberValue(KIO::UDSEntry::UDS_FILE_TYPE))
                           ? EntryType::Dir
                           : EntryType::Image;
  if (entry->m_entryType == EntryType::Dir) {
    QString path = entry->m_url.path(QUrl::FullyEncoded);
    if (!path.endsWith(QLatin1Char('/')))
      entry->m_url.setPath(path + QLatin1Char('/'), QUrl::StrictMode);
  }
  return entry;
}
#endif

DirectoryEntry::EntryType DirectoryEntry::entryType() const {
  return m_entryType;
}

void DirectoryEntry::requestThumbnail() {
  if (m_thumbnailPending)
    return;
  m_thumbnailPending = true;

#if defined(USE_LIBARCHIVE)
  QImage image;
  if (m_url.isLocalFile()) {
    QImageReader reader(m_url.toLocalFile());
    reader.setAutoTransform(true);
    image = reader.read();
  }
  if (!image.isNull()) {
    image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    m_thumbnail = QPixmap::fromImage(image);
  }
  m_thumbnailPending = false;
  if (!m_thumbnail.isNull())
    emit thumbnailReady();
#elif defined(USE_KIO)
  qDebug() << "load url:" << m_url;
  auto *job = KIO::storedGet(m_url, KIO::NoReload);
  connect(job, &KIO::StoredTransferJob::result, this, [this, job]() {
    QImage image;
    if (job->error() == 0) {
      const QByteArray bytes = job->data();
      QBuffer buffer;
      buffer.setData(bytes);
      buffer.open(QIODevice::ReadOnly);
      QImageReader reader(&buffer);
      reader.setAutoTransform(true);
      image = reader.read();
    } else {
      qDebug() << "error loading image:" << job->errorText();
      return;
    }
    if (!image.isNull()) {
      image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
      m_thumbnail = QPixmap::fromImage(image);
    }
    m_thumbnailPending = false;
    if (!m_thumbnail.isNull())
      emit thumbnailReady();
  });
#endif
}

#ifdef USE_QT_PDF
void PdfDirectoryEntry::requestThumbnail() {
  if (m_thumbnailPending)
    return;
  m_thumbnailPending = true;

  QImage image = renderPage();
  if (!image.isNull()) {
    image = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    m_thumbnail = QPixmap::fromImage(image);
  }
  m_thumbnailPending = false;
  if (!m_thumbnail.isNull())
    emit thumbnailReady();
}

QImage PdfDirectoryEntry::renderPage() const {
  if (!m_document || m_document->status() != QPdfDocument::Status::Ready)
    return {};
  QSizeF pageSize = m_document->pagePointSize(m_index);
  QSize imageSize = (pageSize * 1.0).toSize();
  if (imageSize.isEmpty())
    imageSize = QSize(1224, 1584);
  QPdfDocumentRenderOptions opts;
  return m_document->render(m_index, imageSize, opts);
}
#endif
