#pragma once
#include <QList>
#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <imgviewer/DirectoryEntry.h>

#ifdef USE_KIO
namespace KIO {
class ListJob;
}
#endif

#ifdef USE_QT_PDF
#include <QPdfDocument>
#endif

enum class SortBy { Name, DateCreated, DateModified };

class Filter : public QObject {
  Q_OBJECT

signals:
  void changed();
  void tagsLoaded();
  void dirEntriesLoaded();

public:
  Filter();

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

  const QUrl &currentPath() const { return m_currentUrl; }
  void setCurrentPath(const QString &path) {
    m_currentUrl = QUrl::fromUserInput(path);
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

  const QList<QSharedPointer<BaseDirectoryEntry>> &dirEntries() const {
    return m_dirEntries;
  }
  void navigateDirectory(const QSharedPointer<DirectoryEntry> entry);

private slots:
  void requestDirectoryEntries();

private:
#ifdef USE_LIBARCHIVE
  bool extractArchive(const QString &archivePath);

  struct ArchiveTemp {
    QTemporaryDir dir;
    QUrl parentUrl;
  };
#endif

#ifdef USE_QT_PDF
  void requestPdfEntries();
#endif

  QString m_search;
  SortBy m_sortBy = SortBy::Name;
  bool m_descending = false;
  bool m_naturalSort = true;
  QUrl m_currentUrl;

  QList<QString> m_tags;
  QMap<QString, QStringList> m_tagMap;

  QList<QSharedPointer<BaseDirectoryEntry>> m_dirEntries;

#ifdef USE_KIO
  KIO::ListJob *m_job = nullptr;
#endif

#ifdef USE_LIBARCHIVE
  std::unique_ptr<ArchiveTemp> m_archiveTemp;
#endif
};
