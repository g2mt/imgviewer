#pragma once
#include <QList>
#include <QObject>
#include <QString>

enum class SortBy { Name, DateCreated, DateModified };

class Filter : public QObject {
  Q_OBJECT
  QString m_search;
  SortBy m_sortBy = SortBy::Name;
  bool m_descending = false;
  bool m_naturalSort = false;
  QList<QString> m_directories;
  QList<QString> m_tags;

public:
signals:
  void changed();

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

  const QList<QString> &directories() const { return m_directories; }
  void setDirectories(const QList<QString> &d) {
    if (m_directories != d) {
      m_directories = d;
      emit changed();
    }
  }

  const QList<QString> &tags() const { return m_tags; }
  void setTags(const QList<QString> &t) {
    if (m_tags != t) {
      m_tags = t;
      emit changed();
    }
  }
};
