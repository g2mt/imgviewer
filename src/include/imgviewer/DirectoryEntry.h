#pragma once
#include <QDateTime>
#include <QImage>
#include <QMetaType>
#include <QObject>
#include <QUrl>
#include <qsharedpointer.h>

#ifdef USE_QT_PDF
#include <QPdfDocument>
#endif

class BaseDirectoryEntry : public QObject {
  Q_OBJECT

  friend class Filter;

public:
  explicit BaseDirectoryEntry(QObject *parent = nullptr) : QObject(parent) {}

  bool isDir() const { return m_isDir; }
  QDateTime birthTime() const { return m_birthTime; }
  QDateTime lastModified() const { return m_lastModified; }

  virtual bool isImagePath() const;
  virtual bool isArchivePath() const;
  virtual QString name() const = 0;

protected:
  bool m_isDir = false;
  QDateTime m_birthTime;
  QDateTime m_lastModified;
};

class DirectoryEntry : public BaseDirectoryEntry {
  Q_OBJECT
public:
  explicit DirectoryEntry(const QUrl &url, QObject *parent = nullptr)
      : BaseDirectoryEntry(parent), m_url(url) {}

  QUrl url() const { return m_url; }

  QString name() const override { return m_url.fileName(); }
  bool isImagePath() const override;
  bool isArchivePath() const override;

private:
  QUrl m_url;
};

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

private:
  int m_index = 0;
  QSharedPointer<QPdfDocument> m_document;
};
