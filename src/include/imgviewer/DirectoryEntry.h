#pragma once
#include <QDateTime>
#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QPixmap>
#include <QUrl>
#include <qsharedpointer.h>

#ifdef USE_QT_PDF
#include <QPdfDocument>
#endif

class BaseDirectoryEntry : public QObject {
  Q_OBJECT

  friend class Filter;

public:
  static constexpr int kThumbnailSize = 96;

  explicit BaseDirectoryEntry(QObject *parent = nullptr) : QObject(parent) {}

  bool isDir() const { return m_isDir; }
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
  bool m_isDir = false;
  QDateTime m_birthTime;
  QDateTime m_lastModified;
  QPixmap m_thumbnail;
  bool m_thumbnailPending = false;
};

class DirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  explicit DirectoryEntry(const QUrl &url, QObject *parent = nullptr)
      : BaseDirectoryEntry(parent), m_url(url) {}

  QUrl url() const { return m_url; }

  EntryType entryType() const override;
  QString name() const override { return m_url.fileName(); }
  void requestThumbnail() override;

private:
  QUrl m_url;
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
