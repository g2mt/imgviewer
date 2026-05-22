#include <imgviewer/DirectoryEntry.h>

#include <QFileInfo>

bool DirectoryEntry::isImagePath() const {
  if (!path.isLocalFile())
    return false;
  const QString ext = QFileInfo(path.fileName()).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool DirectoryEntry::isArchivePath() const {
  if (!path.isLocalFile())
    return false;
  const QString ext = QFileInfo(path.fileName()).suffix().toLower();
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}
