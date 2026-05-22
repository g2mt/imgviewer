#include <imgviewer/Filter.h>
#include <imgviewer/utils.h>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <qlogging.h>

static QStringList parseCsvLine(const QString &line) {
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
      m_archive.archiveRoot = archiveUrlForPath(fullPath);
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
