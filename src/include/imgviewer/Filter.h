#pragma once
#include <QDir>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

enum class SortBy { Name, DateCreated, DateModified };

class Filter : public QObject {
  Q_OBJECT
  QString m_search;
  SortBy m_sortBy = SortBy::Name;
  bool m_descending = false;
  bool m_naturalSort = false;
  QDir m_currentPath;
  QList<QString> m_tags;
  QMap<QString, QStringList> m_tagMap;

public:
signals:
  void changed();
  void tagsLoaded();

public:
  QString search() const { return m_search; }
  void setSearch(const QString &s) {
    if (m_search != s) {
      m_search = s;
      emit changed();
    }
  }

  SortBy sortBy() const { return m_sortBy; }
  void setSortBy(SortBy s) {
    if (m_sortBy != s) {
      m_sortBy = s;
      emit changed();
    }
  }

  bool descending() const { return m_descending; }
  void setDescending(bool d) {
    if (m_descending != d) {
      m_descending = d;
      emit changed();
    }
  }

  bool naturalSort() const { return m_naturalSort; }
  void setNaturalSort(bool n) {
    if (m_naturalSort != n) {
      m_naturalSort = n;
      emit changed();
    }
  }

  const QDir &currentPath() const { return m_currentPath; }
  void setCurrentPath(const QString &path) {
    m_currentPath.setPath(path);
    emit changed();
  }
  void navigateDirectory(const QString &directory) {
    if (directory == "..") {
      m_currentPath.cdUp();
    } else {
      m_currentPath.cd(directory);
    }
    emit changed();
  }

  const QList<QString> &tags() const { return m_tags; }
  void setTags(const QList<QString> &t) {
    if (m_tags != t) {
      m_tags = t;
      emit changed();
    }
  }

  const QMap<QString, QStringList> &tagMap() const { return m_tagMap; }
  void loadTagsFile(const QString &tagsPath, const QString &pathReplace);
  bool fileHasTags(const QString &filePath) const;
};
