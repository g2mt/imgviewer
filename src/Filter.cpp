#include <imgviewer/Filter.h>
#include <imgviewer/utils.h>

#include <QFile>
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
  m_archive.tempDir.reset();
  m_archive.sourceDir.clear();
  m_currentPath.setPath(path);
  emit changed();
}

void Filter::navigateDirectory(const QString &directory) {
  if (directory == "..") {
    if (m_archive.tempDir &&
        m_currentPath.absolutePath() == m_archive.tempDir->path()) {
      // At the root of an archive temp dir - go back to the archive source.
      m_archive.tempDir.reset();
      m_currentPath.setPath(m_archive.sourceDir);
      m_archive.sourceDir.clear();
      emit changed();
      return;
    }
    m_currentPath.cdUp();
    emit changed();
    return;
  }

  // When not already inside an archive, try to extract archive files.
  if (!m_archive.tempDir) {
    const QString fullPath = m_currentPath.absoluteFilePath(directory);
    const QFileInfo info(fullPath);
    if (info.isFile() && isArchivePath(fullPath)) {
      auto tempDir = std::make_unique<QTemporaryDir>();
      if (tempDir->isValid() && extractArchiveTo(fullPath, tempDir->path())) {
        m_archive.sourceDir = m_currentPath.absolutePath();
        m_archive.tempDir = std::move(tempDir);
        m_currentPath.setPath(m_archive.tempDir->path());
        emit changed();
        return;
      }
    }
  }

  m_currentPath.cd(directory);
  emit changed();
}
