#include <imgviewer/utils.h>
#include <QFileInfo>

#ifdef IMGVIEWER_FEAT_ARCHIVE
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFile>
#endif

bool isImagePath(const QString &path) {
  const QString ext = QFileInfo(path).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool isArchivePath(const QString &path) {
  const QString ext = QFileInfo(path).suffix().toLower();
  return ext == "zip" || ext == "tar" || ext == "tgz" ||
         ext == "tbz2" || ext == "txz" || ext == "7z" ||
         ext == "rar" || ext == "gz" || ext == "bz2" ||
         ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}

#ifdef IMGVIEWER_FEAT_ARCHIVE
bool extractArchiveTo(const QString &archivePath, const QString &destDir) {
  struct archive *a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);

  if (archive_read_open_filename(a, archivePath.toUtf8().constData(), 10240) !=
      ARCHIVE_OK) {
    archive_read_free(a);
    return false;
  }

  struct archive_entry *entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    QString entryPath = QString::fromUtf8(archive_entry_pathname(entry));
    while (entryPath.startsWith(QLatin1Char('/')))
      entryPath.remove(0, 1);
    if (entryPath.contains(QLatin1String("..")))
      continue;

    const QString fullPath = destDir + QLatin1Char('/') + entryPath;

    mode_t fileType = archive_entry_filetype(entry);
    if (fileType == AE_IFDIR) {
      QDir().mkpath(fullPath);
    } else if (fileType == AE_IFREG) {
      QDir().mkpath(QFileInfo(fullPath).absolutePath());
      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly))
        continue;

      const void *buf;
      size_t size;
      la_int64_t offset;
      while (archive_read_data_block(a, &buf, &size, &offset) == ARCHIVE_OK)
        file.write(static_cast<const char *>(buf), size);
    }
  }

  int ret = archive_read_close(a);
  archive_read_free(a);
  return ret == ARCHIVE_OK;
}
#else
bool extractArchiveTo(const QString &archivePath, const QString &destDir) {
  Q_UNUSED(archivePath);
  Q_UNUSED(destDir);
  return false;
}
#endif
