#include <imgviewer/Filter.h>

#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <sys/stat.h>

#ifdef USE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#elif defined(USE_KIO)
#include <KIO/ListJob>
#include <KIO/StoredTransferJob>
#endif

namespace {

QStringList parseCsvLine(const QString &line) {
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

} // namespace

/** Tags **/

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

#ifdef USE_LIBARCHIVE
bool Filter::extractArchive(const QString &archivePath) {
  struct archive *a = archive_read_new();
  archive_read_support_filter_all(a);
  archive_read_support_format_all(a);

  if (archive_read_open_filename(a, QFile::encodeName(archivePath).constData(),
                                 10240) != ARCHIVE_OK) {
    archive_read_free(a);
    return false;
  }

  QString destDir = m_archiveTemp->dir.path();
  struct archive_entry *entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    const QString name = QString::fromUtf8(archive_entry_pathname(entry));
    if (name.isEmpty())
      continue;

    const QString fullPath = destDir + QLatin1Char('/') + name;

    if (archive_entry_filetype(entry) == AE_IFREG) {
      QFileInfo fi(fullPath);
      QDir().mkpath(fi.absolutePath());

      FILE *f = fopen(QFile::encodeName(fullPath).constData(), "wb");
      if (!f) {
        archive_read_free(a);
        return false;
      }
      char buf[8192];
      la_ssize_t len;
      while ((len = archive_read_data(a, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, len, f) != (size_t)len) {
          fclose(f);
          archive_read_free(a);
          return false;
        }
      }
      fclose(f);
    } else if (archive_entry_filetype(entry) == AE_IFDIR) {
      QDir().mkpath(fullPath);
    }
  }

  archive_read_free(a);
  return true;
}
#endif

/** Directory **/

QList<QSharedPointer<BaseDirectoryEntry>> Filter::listDirectoryEntries() {
  QList<QSharedPointer<BaseDirectoryEntry>> entries;

#ifdef USE_QT_PDF
  if (m_currentUrl.isLocalFile() &&
      m_currentUrl.fileName().endsWith(QLatin1String(".pdf"),
                                       Qt::CaseInsensitive)) {
    return listPdfEntries();
  }
#endif

  bool isLocal = m_currentUrl.scheme().isEmpty() ||
                 m_currentUrl.scheme() == QLatin1String("file") ||
                 m_currentUrl.isLocalFile();

  if (isLocal) {
    QString local = m_currentUrl.isLocalFile() ? m_currentUrl.toLocalFile()
                                               : m_currentUrl.path();
    QDir dir(local);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
      if (info.fileName() == "." || info.fileName() == "..")
        continue;
      auto entry = QSharedPointer<DirectoryEntry>::create(
          QUrl::fromLocalFile(info.absoluteFilePath()));
      entry->m_isDir = info.isDir();
      entry->m_birthTime = info.birthTime();
      entry->m_lastModified = info.lastModified();
      entries.append(entry);
    }
    return entries;
  }

#ifdef USE_KIO
  KIO::ListJob *job = KIO::listDir(m_currentUrl, KIO::HideProgressInfo);
  QEventLoop loop;

  QObject::connect(
      job, &KIO::ListJob::entries, &loop, [&](auto, const auto &list) {
        for (const auto &uds : list) {
          const QString name = uds.stringValue(KIO::UDSEntry::UDS_NAME);
          if (name.isEmpty() || name == "." || name == "..")
            continue;
          auto entry = QSharedPointer<DirectoryEntry>::create(
              m_currentUrl.resolved(name));
          const mode_t mode = uds.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
          entry->m_isDir = S_ISDIR(mode);
          entries.append(entry);
        }
      });
  QObject::connect(job, &KIO::ListJob::result, &loop,
                   [&loop](auto...) { loop.quit(); });
  loop.exec();
#endif
  return entries;
}

void Filter::setCurrentPath(const QString &path) {
  m_currentUrl = QUrl::fromUserInput(path);
  emit changed();
}

void Filter::navigateDirectory(const DirectoryEntry &entry) {
  const QUrl &url = entry->url();

  // Handle ".." — navigate to parent directory
  if (url.toString() == QLatin1String("..")) {
#if defined(USE_LIBARCHIVE)
    if (m_archiveTemp) {
      QUrl parentUrl = m_currentUrl.resolved(QUrl(".."));
      if (QUrl::fromLocalFile(m_archiveTemp->dir.path())
              .isParentOf(parentUrl)) {
        // Still inside the archive
        m_currentUrl = parentUrl;
        emit changed();
        return;
      }
      // Leaving the archive entirely
      m_currentUrl = m_archiveTemp->parentUrl;
      m_archiveTemp.reset();
      emit changed();
      return;
    }
#elif defined(USE_KIO)
    // Inside a zip archive: walk up the virtual path, fall back to local file
    // system
    if (m_currentUrl.scheme() == QLatin1String("zip")) {
      m_currentUrl = m_currentUrl.adjusted(QUrl::StripTrailingSlash);
      m_currentUrl = m_currentUrl.adjusted(QUrl::RemoveFilename);
      if (QFile::exists(m_currentUrl.path()))
        m_currentUrl.setScheme("file");
      emit changed();
      return;
    }
#endif
    // Local file: use QDir::cdUp
    if (m_currentUrl.isLocalFile()) {
      QDir dir(m_currentUrl.toLocalFile());
      dir.cdUp();
      m_currentUrl = QUrl::fromLocalFile(dir.absolutePath());
    } else {
      // Remote URL: strip the last path segment
      m_currentUrl = m_currentUrl.adjusted(QUrl::StripTrailingSlash);
      m_currentUrl = m_currentUrl.adjusted(QUrl::RemoveFilename);
    }
    emit changed();
    return;
  }

  // First navigation: current path is empty, accept the entry as-is
  if (m_currentUrl.isEmpty()) {
    m_currentUrl = url;
    emit changed();
    return;
  }

#ifdef USE_QT_PDF
  if (url.isLocalFile() &&
      url.fileName().endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
    m_currentUrl = url;
    emit changed();
    return;
  }
#endif

#if defined(USE_LIBARCHIVE)
  // Local entry that looks like an archive — extract it to a temp directory
  if (entry.isArchivePath()) {
    auto temp = std::make_unique<ArchiveTemp>();
    if (temp->dir.isValid()) {
      temp->parentUrl = m_currentUrl;
      m_archiveTemp = std::move(temp);
      if (extractArchive(url.toLocalFile())) {
        m_currentUrl = QUrl::fromLocalFile(m_archiveTemp->dir.path());
        emit changed();
        return;
      }
      // Extraction failed, clean up
      m_archiveTemp.reset();
    }
  }
#elif defined(USE_KIO)
  // Local entry that looks like an archive — navigate into it via zip:// scheme
  if (entry.isArchivePath()) {
    QUrl archiveUrl(url);
    archiveUrl.setScheme(QStringLiteral("zip"));
    if (!archiveUrl.path().endsWith("/"))
      archiveUrl.setPath(archiveUrl.path() + "/");
    m_currentUrl = archiveUrl;
    emit changed();
    return;
  }
#endif

  // Normal directory navigation: switch to the entry's path
  m_currentUrl = url;
  emit changed();
}

#ifdef USE_QT_PDF

QList<QSharedPointer<BaseDirectoryEntry>> Filter::listPdfEntries() {
  QList<QSharedPointer<BaseDirectoryEntry>> entries;

  auto pdfDocument = QSharedPointer<QPdfDocument>(new QPdfDocument());
  auto err = pdfDocument->load(m_currentUrl.toLocalFile());
  if (err != QPdfDocument::Error::None ||
      pdfDocument->status() != QPdfDocument::Status::Ready)
    return entries;

  int pageCount = pdfDocument->pageCount();
  entries.reserve(pageCount);

  for (int i = 0; i < pageCount; ++i) {
    auto pdfEntry = QSharedPointer<PdfDirectoryEntry>::create();
    pdfEntry->setPageIndex(i);
    pdfEntry->setPdfDocument(pdfDocument);
    entries.append(pdfEntry);
  }

  return entries;
}

#endif
