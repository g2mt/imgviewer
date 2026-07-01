#pragma once
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QPixmap>
#include <QUrl>
#include <qsharedpointer.h>

#ifdef USE_KIO
namespace KIO {
class UDSEntry;
}
#endif

#ifdef USE_QT_PDF
#include <QPdfDocument>
#endif

class BaseDirectoryEntry : public QObject {
  Q_OBJECT

  friend class Filter;

public:
  static constexpr int kThumbnailSize = 96;

  explicit BaseDirectoryEntry(QObject *parent = nullptr) : QObject(parent) {}

  QDateTime birthTime() const { return m_birthTime; }
  QDateTime lastModified() const { return m_lastModified; }

  QPixmap thumbnail() const { return m_thumbnail; }
  bool hasThumbnail() const { return !m_thumbnail.isNull(); }
  virtual void requestThumbnail();

  enum class EntryType { Dir, Image, Archive };
  virtual EntryType entryType() const;
  virtual QString name() const = 0;

signals:
  void thumbnailReady();

protected:
  QDateTime m_birthTime;
  QDateTime m_lastModified;
  QPixmap m_thumbnail;
  bool m_thumbnailPending = false;
};

class DirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  static DirectoryEntry *fromFileInfo(const QFileInfo &info,
                                        QObject *parent = nullptr);
#ifdef USE_KIO
  static DirectoryEntry *fromKio(const KIO::UDSEntry &uds,
                                  const QUrl &parentDir,
                                  QObject *parent = nullptr);
#endif

  QUrl url() const { return m_url; }

  EntryType entryType() const override;
  QString name() const override { return m_name; }
  void requestThumbnail() override;

protected:
  explicit DirectoryEntry(QObject *parent = nullptr) : BaseDirectoryEntry(parent) {}

  QUrl m_url;
  QString m_name;
  EntryType m_entryType = EntryType::Dir;
};

class UpDirectoryEntry : public DirectoryEntry {
  Q_OBJECT
public:
  explicit UpDirectoryEntry(QObject *parent = nullptr)
      : DirectoryEntry(parent) {
    m_url = QUrl(QStringLiteral(".."));
    m_name = QStringLiteral("..");
    m_entryType = EntryType::Dir;
  }

  QString name() const override { return QStringLiteral(".."); }
};

#ifdef USE_QT_PDF
class PdfDirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  explicit PdfDirectoryEntry(QObject *parent = nullptr)
      : BaseDirectoryEntry(parent) {}

  int pageIndex() const { return m_index; }
  void setPageIndex(int i) { m_index = i; }
  QSharedPointer<QPdfDocument> pdfDocument() const { return m_document; }
  void setPdfDocument(const QSharedPointer<QPdfDocument> &doc) {
    m_document = doc;
  }

  QString name() const override {
    return QStringLiteral("Page %1").arg(m_index + 1, 2, 10, QLatin1Char('0'));
  }

  QImage renderPage() const;
  void requestThumbnail() override;

private:
  int m_index = 0;
  QSharedPointer<QPdfDocument> m_document;
};
#endif
