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
  explicit BaseDirectoryEntry(QObject *parent = nullptr) : QObject(parent) {}

  QDateTime m_birthTime;
  QDateTime m_lastModified;
  QPixmap m_thumbnail;
  bool m_thumbnailPending = false;
};

class DirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  // Ownership is managed by QSharedPointer, no parent argument needed.
  static QSharedPointer<DirectoryEntry> fromFileInfo(const QFileInfo &info);
#ifdef USE_KIO
  static QSharedPointer<DirectoryEntry> fromKio(const KIO::UDSEntry &uds,
                                                const QUrl &parentDir);
#endif

  QUrl url() const { return m_url; }
  EntryType entryType() const override;
  QString name() const override { return m_name; }
  void requestThumbnail() override;

protected:
  explicit DirectoryEntry(QObject *parent = nullptr)
      : BaseDirectoryEntry(parent) {}

  QUrl m_url;
  QString m_name;
  EntryType m_entryType = EntryType::Dir;
};

class UpDirectoryEntry : public DirectoryEntry {
  Q_OBJECT
public:
  // Ownership is managed by QSharedPointer, no parent argument needed.
  static QSharedPointer<UpDirectoryEntry> create() {
    auto *entry = new UpDirectoryEntry();
    return QSharedPointer<UpDirectoryEntry>(entry);
  }

  QString name() const override { return QStringLiteral(".."); }

private:
  explicit UpDirectoryEntry() : DirectoryEntry() {
    m_url = QUrl(QStringLiteral(".."));
    m_name = QStringLiteral("..");
    m_entryType = EntryType::Dir;
  }
};

#ifdef USE_QT_PDF
class PdfDirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  // Ownership is managed by QSharedPointer, no parent argument needed.
  static QSharedPointer<PdfDirectoryEntry>
  create(int pageIndex, const QSharedPointer<QPdfDocument> &document) {
    auto *entry = new PdfDirectoryEntry(pageIndex, document);
    return QSharedPointer<PdfDirectoryEntry>(entry);
  }

  int pageIndex() const { return m_index; }
  QSharedPointer<QPdfDocument> pdfDocument() const { return m_document; }

  QString name() const override {
    return QStringLiteral("Page %1").arg(m_index + 1, 2, 10, QLatin1Char('0'));
  }

  QImage renderPage() const;
  void requestThumbnail() override;

  EntryType entryType() const override { return EntryType::Image; }

private:
  explicit PdfDirectoryEntry(int pageIndex,
                             const QSharedPointer<QPdfDocument> &document,
                             QObject *parent = nullptr)
      : BaseDirectoryEntry(parent), m_index(pageIndex), m_document(document) {}
  int m_index;
  QSharedPointer<QPdfDocument> m_document;
};
#endif
