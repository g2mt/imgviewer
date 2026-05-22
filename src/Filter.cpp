#include <imgviewer/Filter.h>

#include <KIO/ListJob>
#include <KIO/StoredTransferJob>
#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
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

QStringList parseCsvLine(const QString &line) {
  QStringList fields;
  QString field;
  bool escaped = false;
  for (int i = 0; i < line.size(); ++i) {
    QChar c = line[i];
    if (escaped) {
      field += c;
      escaped = false;
    } else if (c == QLatin1Char('\\')) {
      escaped = true;
    } else if (c == QLatin1Char(',')) {
      fields.append(field);
      field.clear();
    } else {
      field += c;
    }
  }
  fields.append(field);
  return fields;
}

} // namespace

QString Filter::resolvePath(const QString &path) const {
  QUrl url(path);
  if (url.fileName() == "." || url.fileName() == "..")
    return {};
  if (url.scheme().isEmpty() && !QFileInfo(path).isAbsolute()) {
    if (m_currentPath.isEmpty())
      return path;
    return childPath(m_currentPath, path);
  }
  return path;
}

bool Filter::isImagePath(const QString &path) const {
  const QString resolved = resolvePath(path);
  if (resolved.isEmpty())
    return false;
  const QString ext = QFileInfo(resolved).suffix().toLower();
  return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
         ext == "gif" || ext == "pbm" || ext == "pgm" || ext == "ppm" ||
         ext == "xbm" || ext == "xpm" || ext == "svg" || ext == "webp" ||
         ext == "tiff" || ext == "tif" || ext == "ico";
}

bool Filter::isArchivePath(const QString &path) const {
  const QString resolved = resolvePath(path);
  if (resolved.isEmpty())
    return false;
  const QString ext = QFileInfo(resolved).suffix().toLower();
  return ext == "zip" || ext == "tar" || ext == "tgz" || ext == "tbz2" ||
         ext == "txz" || ext == "7z" || ext == "rar" || ext == "gz" ||
         ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lzma" ||
         ext == "zst" || ext == "iso" || ext == "cpio" || ext == "ar";
}

QString Filter::childPath(const QString &basePath,
                          const QString &childName) const {
  if (isLocalPath(basePath))
    return QDir(localPath(basePath)).filePath(childName);

  QUrl url = urlFromPath(basePath);
  QString path = url.path();
  if (!path.endsWith(QLatin1Char('/')))
    path += QLatin1Char('/');
  url.setPath(path + childName);
  return url.toString();
}

QString Filter::parentPath(const QString &path) const {
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

QList<DirectoryEntry> Filter::listDirectoryEntries(const QString &path) const {
  QList<DirectoryEntry> entries;

  if (isLocalPath(path)) {
    QDir dir(localPath(path));
    const QFileInfoList infos = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
      DirectoryEntry entry;
      entry.name = info.fileName();
      if (entry.name == "." || entry.name == "..")
        continue;
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

  QObject::connect(
      job, &KIO::ListJob::entries, &loop, [&](auto, const auto &list) {
        for (const auto &uds : list) {
          DirectoryEntry entry;
          entry.name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
          if (entry.name.isEmpty())
            continue;
          const mode_t mode = uds.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
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

QByteArray Filter::readFileBytes(const QString &path) const {
  const QString resolved = resolvePath(path);
  if (resolved.isEmpty())
    return {};
  if (isLocalPath(resolved)) {
    QFile file(localPath(resolved));
    if (!file.open(QIODevice::ReadOnly))
      return {};
    return file.readAll();
  }

  KIO::StoredTransferJob *job =
      KIO::storedGet(urlFromPath(resolved), KIO::NoReload);
  QEventLoop loop;
  QObject::connect(job, &KIO::StoredTransferJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
  if (job->error())
    return {};
  return job->data();
}

void Filter::loadTagsFile(const QString &tagsPath, const QString &pathReplace) {
  m_tagMap.clear();

  QString fromPrefix, toPrefix;
  if (!pathReplace.isEmpty()) {
    QStringList parts = pathReplace.split(QLatin1Char(':'));
    if (parts.size() == 2) {
      fromPrefix = parts[0];
      toPrefix = parts[1];
    }
  }

  QFile file(tagsPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty())
      continue;

    QStringList fields = parseCsvLine(line);
    if (fields.size() < 2)
      continue;

    QString path = fields[0];
    if (!fromPrefix.isEmpty() && path.startsWith(fromPrefix))
      path = toPrefix + path.mid(fromPrefix.size());

    for (int i = 1; i < fields.size(); ++i) {
      const QString &tag = fields[i];
      if (!tag.isEmpty())
        m_tagMap[tag].append(path);
    }
  }

  emit tagsLoaded();
}

bool Filter::fileHasTags(const QString &filePath) const {
  if (m_tags.isEmpty())
    return true;
  for (const QString &tag : m_tags) {
    auto it = m_tagMap.constFind(tag);
    if (it == m_tagMap.constEnd())
      return false;
    if (!it->contains(filePath))
      return false;
  }
  return true;
}

void Filter::setCurrentPath(const QString &path) {
  m_archive.sourceDir.clear();
  m_archive.archiveRoot.clear();
  m_currentPath = path;
  emit changed();
}

void Filter::navigateDirectory(const QString &directory) {
  if (directory == "..") {
    if (!m_archive.archiveRoot.isEmpty() &&
        m_currentPath == m_archive.archiveRoot) {
      m_currentPath = m_archive.sourceDir;
      m_archive.sourceDir.clear();
      m_archive.archiveRoot.clear();
      emit changed();
      return;
    }
    m_currentPath = parentPath(m_currentPath);
    emit changed();
    return;
  }

  if (m_currentPath.isEmpty()) {
    m_currentPath = directory;
    emit changed();
    return;
  }

  if (m_archive.archiveRoot.isEmpty()) {
    const QString fullPath = directory.contains(QLatin1Char('/')) ||
                                     directory.contains(QLatin1String(":"))
                                 ? directory
                                 : childPath(m_currentPath, directory);
    const QFileInfo info(fullPath);
    if (info.isFile() && isArchivePath(fullPath)) {
      m_archive.sourceDir = m_currentPath;
      QUrl url;
      url.setScheme(QStringLiteral("zip"));
      url.setPath(fullPath);
      m_archive.archiveRoot = url.toString();
      m_currentPath = m_archive.archiveRoot;
      emit changed();
      return;
    }

    m_currentPath = fullPath;
    emit changed();
    return;
  }

  m_currentPath = directory.contains(QLatin1Char('/')) ||
                          directory.contains(QLatin1String(":"))
                      ? directory
                      : childPath(m_currentPath, directory);
  emit changed();
}
