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

/** Tags **/

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

/** Directory **/

QList<DirectoryEntry> Filter::listDirectoryEntries() const {
  QList<DirectoryEntry> entries;

  bool isLocal = m_currentUrl.scheme().isEmpty() ||
                 m_currentUrl.scheme() == QLatin1String("file") ||
                 m_currentUrl.isLocalFile();

  if (isLocal) {
    QString local = m_currentUrl.isLocalFile() ? m_currentUrl.toLocalFile()
                                               : m_currentUrl.path();
    QDir dir(local);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
      DirectoryEntry entry;
      if (info.fileName() == "." || info.fileName() == "..")
        continue;
      entry.path = QUrl::fromLocalFile(info.absoluteFilePath());
      entry.isDir = info.isDir();
      entry.birthTime = info.birthTime();
      entry.lastModified = info.lastModified();
      entries.append(entry);
    }
    return entries;
  }

  QUrl url = m_currentUrl;
  if (url.scheme().isEmpty())
    url = QUrl::fromLocalFile(url.path());
  KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
  QEventLoop loop;

  QObject::connect(
      job, &KIO::ListJob::entries, &loop, [&](auto, const auto &list) {
        for (const auto &uds : list) {
          DirectoryEntry entry;
          const QString name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
          if (name.isEmpty())
            continue;
          const mode_t mode = uds.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
          entry.isDir = S_ISDIR(mode);
          QString basePath = url.path();
          if (!basePath.endsWith(QLatin1Char('/')))
            basePath += QLatin1Char('/');
          QUrl childUrl(url);
          childUrl.setPath(basePath + name);
          entry.path = childUrl;
          entries.append(entry);
        }
      });
  QObject::connect(job, &KIO::ListJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
  return entries;
}

void Filter::setCurrentPath(const QString &path) {
  m_currentUrl = QUrl::fromUserInput(path);
  emit changed();
}

void Filter::navigateDirectory(const DirectoryEntry &entry) {
  // Handle ".." — navigate to parent directory
  if (entry.path.toString() == QLatin1String("..")) {
    // Inside a zip archive: walk up the virtual path, fall back to local file
    // system
    if (m_currentUrl.scheme() == QLatin1String("zip")) {
      QUrl url = m_currentUrl.resolved(QUrl(".."));
      m_currentUrl = url;
      if (QFile::exists(url.path())) {
        m_currentUrl.setScheme("file");
      }
      emit changed();
      return;
    }

    // Local file: use QDir::cdUp
    if (m_currentUrl.isLocalFile()) {
      QDir dir(m_currentUrl.toLocalFile());
      dir.cdUp();
      m_currentUrl = QUrl::fromLocalFile(dir.absolutePath());
    } else {
      // Remote URL: strip the last path segment
      m_currentUrl = m_currentUrl.adjusted(QUrl::RemoveFilename |
                                           QUrl::StripTrailingSlash);
    }
    emit changed();
    return;
  }

  // First navigation: current path is empty, accept the entry as-is
  if (m_currentUrl.isEmpty()) {
    m_currentUrl = entry.path;
    emit changed();
    return;
  }

  // Local entry that looks like an archive — navigate into it via zip:// scheme
  if (entry.path.isLocalFile()) {
    DirectoryEntry checkEntry;
    checkEntry.path = entry.path;
    if (checkEntry.isArchivePath()) {
      QUrl archiveUrl;
      archiveUrl.setScheme(QStringLiteral("zip"));
      archiveUrl.setPath(entry.path.toLocalFile());
      m_currentUrl = archiveUrl;
      emit changed();
      return;
    }
  }

  // Normal directory navigation: switch to the entry's path
  m_currentUrl = entry.path;
  emit changed();
}
