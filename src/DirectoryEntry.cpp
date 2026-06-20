#include <imgviewer/DirectoryEntry.h>

#include <QFileInfo>

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

#ifdef USE_QT_PDF
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
