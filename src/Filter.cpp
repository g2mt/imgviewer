#include <imgviewer/Filter.h>

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
