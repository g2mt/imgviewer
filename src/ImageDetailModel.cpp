#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>

#include <QCollator>
#include <algorithm>

ImageDetailModel::ImageDetailModel(Filter *filter, QObject *parent)
    : QAbstractListModel(parent), m_filter(filter) {
  populate();
  connect(m_filter, &Filter::dirEntriesLoaded, this,
          &ImageDetailModel::populate);
}

int ImageDetailModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_filteredIndices.size();
}

QVariant ImageDetailModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= m_filteredIndices.size())
    return {};
  const auto &entries = m_filter->dirEntries();
  const auto &entry = entries[m_filteredIndices[index.row()]];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return entry->name();
  case FilePathRole: {
#ifdef USE_QT_PDF
    if (auto *pdfEntry = qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return QVariant::fromValue(pdfEntry);
#endif
    return qobject_cast<DirectoryEntry *>(entry.data())->url();
  }
  case ThumbnailRole: {
    if (entry->hasThumbnail())
      return entry->thumbnail();
    entry->requestThumbnail();
    return {};
  }
  default:
    return {};
  }
}

void ImageDetailModel::populate() {
  beginResetModel();
  m_filteredIndices.clear();

  const auto &entries = m_filter->dirEntries();
  const QString search = m_filter->search();
  const QList<QString> tags = m_filter->tags();

  // Build list of indices that pass the filter
  for (int i = 0; i < entries.size(); ++i) {
    const auto &entry = entries[i];
#ifdef USE_QT_PDF
    if (qobject_cast<PdfDirectoryEntry *>(entry.data())) {
      m_filteredIndices.append(i);
      continue;
    }
#endif
    if (entry->isDir())
      continue;
    else if (!entry->isImagePath())
      continue;
    else if (!search.isEmpty() &&
             !entry->name().contains(search, Qt::CaseInsensitive))
      continue;
    else if (!tags.isEmpty()) {
      auto *de = qobject_cast<DirectoryEntry *>(entry.data());
      if (de && !m_filter->fileHasTags(de->url().toString()))
        continue;
    }
    m_filteredIndices.append(i);
  }

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(m_filter->naturalSort());

  const SortBy sortBy = m_filter->sortBy();
  const bool descending = m_filter->descending();
  std::sort(m_filteredIndices.begin(), m_filteredIndices.end(),
            [&](int a, int b) {
              const auto &entryA = entries[a];
              const auto &entryB = entries[b];
              bool less = false;
              switch (sortBy) {
              case SortBy::Name:
                less = collator.compare(entryA->name(), entryB->name()) < 0;
                break;
              case SortBy::DateCreated:
                less = entryA->birthTime() < entryB->birthTime();
                break;
              case SortBy::DateModified:
                less = entryA->lastModified() < entryB->lastModified();
                break;
              }
              return descending ? !less : less;
            });

  for (int i = 0; i < m_filteredIndices.size(); ++i) {
    const auto &entry = entries[m_filteredIndices[i]];
    connect(entry.data(), &BaseDirectoryEntry::thumbnailReady, this,
            [this, i]() {
              const QModelIndex idx = index(i);
              emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
            });
  }

  endResetModel();
}
