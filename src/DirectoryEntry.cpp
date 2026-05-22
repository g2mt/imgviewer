#include <imgviewer/DirectoryEntry.h>

#include <KIO/StoredTransferJob>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>

namespace {

bool isLocalPath(const QUrl &url) {
  return url.scheme().isEmpty() || url.scheme() == QLatin1String("file") ||
         url.isLocalFile();
}

QString localPath(const QUrl &url) {
  return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

} // namespace

bool DirectoryEntry::isImagePath() const {
  const QString ext = QFileInfo(path.fileName()).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool DirectoryEntry::isArchivePath() const {
  const QString ext = QFileInfo(path.fileName()).suffix().toLower();
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}

QByteArray DirectoryEntry::readFileBytes() const {
  if (isLocalPath(path)) {
    QFile file(localPath(path));
    if (!file.open(QIODevice::ReadOnly))
      return {};
    return file.readAll();
  }

  KIO::StoredTransferJob *job = KIO::storedGet(path, KIO::NoReload);
  QEventLoop loop;
  QObject::connect(job, &KIO::StoredTransferJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
  if (job->error())
    return {};
  return job->data();
}
