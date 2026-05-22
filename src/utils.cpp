#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <imgviewer/utils.h>

#include <KIO/ListJob>
#include <KIO/StoredTransferJob>
#include <QEventLoop>
#include <sys/stat.h>

namespace {

QUrl urlFromPath(const QString &path) {
  QUrl url(path);
  if (url.scheme().isEmpty() || url.scheme() == QLatin1String("file"))
    return QUrl::fromLocalFile(url.isLocalFile() ? url.toLocalFile() : path);
  return url;
}

bool isLocalPath(const QString &path) {
  const QUrl url(path);
  return url.scheme().isEmpty() || url.scheme() == QLatin1String("file") ||
         url.isLocalFile();
}

QString localPath(const QString &path) {
  const QUrl url(path);
  return url.isLocalFile() ? url.toLocalFile() : path;
}

} // namespace

bool isImagePath(const QString &path) {
  const QString ext = QFileInfo(path).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool isArchivePath(const QString &path) {
  const QString ext = QFileInfo(path).suffix().toLower();
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}

QString childPath(const QString &basePath, const QString &childName) {
  if (isLocalPath(basePath))
    return QDir(localPath(basePath)).filePath(childName);

  QUrl url = urlFromPath(basePath);
  QString path = url.path();
  if (!path.endsWith(QLatin1Char('/')))
    path += QLatin1Char('/');
  url.setPath(path + childName);
  return url.toString();
}

QString parentPath(const QString &path) {
  if (isLocalPath(path)) {
    QDir dir(localPath(path));
    dir.cdUp();
    return dir.absolutePath();
  }

  QUrl url = urlFromPath(path);
  QString current = url.path();
  if (current.isEmpty() || current == QLatin1String("/"))
    return {};
  if (current.endsWith(QLatin1Char('/')) && current.size() > 1)
    current.chop(1);
  url.setPath(current);
  url = url.adjusted(QUrl::RemoveFilename);
  return url.toString();
}

QString archiveUrlForPath(const QString &path) {
  QUrl url;
  url.setScheme(QStringLiteral("zip"));
  url.setPath(path);
  return url.toString();
}

QList<DirectoryEntry> listDirectoryEntries(const QString &path) {
  QList<DirectoryEntry> entries;

  if (isLocalPath(path)) {
    QDir dir(localPath(path));
    const QFileInfoList infos =
        dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                          QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
      DirectoryEntry entry;
      entry.name = info.fileName();
      entry.path = info.absoluteFilePath();
      entry.isDir = info.isDir();
      entry.birthTime = info.birthTime();
      entry.lastModified = info.lastModified();
      entries.append(entry);
    }
    return entries;
  }

  const QUrl url = urlFromPath(path);
  KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
  QEventLoop loop;

  QObject::connect(job, &KIO::ListJob::entries, &loop,
                   [&](auto, const auto &list) {
                     for (const auto &uds : list) {
                       DirectoryEntry entry;
                       entry.name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
                       if (entry.name.isEmpty())
                         continue;
                       const mode_t mode =
                           uds.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
                       entry.isDir = S_ISDIR(mode);
                       entry.path = childPath(path, entry.name);
                       entries.append(entry);
                     }
                   });
  QObject::connect(job, &KIO::ListJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
  return entries;
}

QByteArray readFileBytes(const QString &path) {
  if (isLocalPath(path)) {
    QFile file(localPath(path));
    if (!file.open(QIODevice::ReadOnly))
      return {};
    return file.readAll();
  }

  KIO::StoredTransferJob *job = KIO::storedGet(urlFromPath(path), KIO::NoReload);
  QEventLoop loop;
  QObject::connect(job, &KIO::StoredTransferJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
  if (job->error())
    return {};
  return job->data();
}
