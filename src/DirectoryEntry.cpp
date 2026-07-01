#include <imgviewer/DirectoryEntry.h>

#ifdef USE_KIO
#include <KIO/StoredTransferJob>
#endif
#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>

void BaseDirectoryEntry::requestThumbnail() {}

bool BaseDirectoryEntry::isImagePath() const { return false; }

bool BaseDirectoryEntry::isArchivePath() const { return false; }

bool DirectoryEntry::isImagePath() const {
  const QString ext = QFileInfo(m_url.fileName()).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool DirectoryEntry::isArchivePath() const {
  const QString ext = QFileInfo(m_url.fileName()).suffix().toLower();
#ifdef USE_QT_PDF
  if (ext == "pdf")
    return true;
#endif
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
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
