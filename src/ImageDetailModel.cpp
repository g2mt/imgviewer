#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailModel.h>

#include <QCollator>
#include <algorithm>

ImageDetailModel::ImageDetailModel(Filter *filter, QObject *parent)
    : QAbstractListModel(parent), m_filter(filter) {
  reload();
  connect(m_filter, &Filter::changed, this, &ImageDetailModel::reload);
}

int ImageDetailModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_files.size();
}

QVariant ImageDetailModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size())
    return {};
  const auto &entry = m_files[index.row()];

  switch (role) {
  case Qt::DisplayRole:
  case FileNameRole:
    return entry->name();
  case FilePathRole: {
    if (auto *pdfEntry = qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return QVariant::fromValue(pdfEntry);
    return qobject_cast<DirectoryEntry *>(entry.data())->url().toString();
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

void ImageDetailModel::reload() {
  beginResetModel();
  m_files.clear();

  auto entries = m_filter->listDirectoryEntries();
  const QString search = m_filter->search();
  const QList<QString> tags = m_filter->tags();
  entries.removeIf([&](const QSharedPointer<BaseDirectoryEntry> &entry) {
    if (qobject_cast<PdfDirectoryEntry *>(entry.data()))
      return false;
    if (entry->isDir())
      return true;
    if (!entry->isImagePath())
      return true;
    if (!search.isEmpty() &&
        !entry->name().contains(search, Qt::CaseInsensitive))
      return true;
    if (!tags.isEmpty()) {
      auto *de = qobject_cast<DirectoryEntry *>(entry.data());
      if (de && !m_filter->fileHasTags(de->url().toString()))
        return true;
    }
    return false;
  });

  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  collator.setNumericMode(m_filter->naturalSort());

  const SortBy sortBy = m_filter->sortBy();
  const bool descending = m_filter->descending();
  std::sort(entries.begin(), entries.end(),
            [&](const QSharedPointer<BaseDirectoryEntry> &a,
                const QSharedPointer<BaseDirectoryEntry> &b) {
              bool less = false;
              switch (sortBy) {
              case SortBy::Name:
                less = collator.compare(a->name(), b->name()) < 0;
                break;
              case SortBy::DateCreated:
                less = a->birthTime() < b->birthTime();
                break;
              case SortBy::DateModified:
                less = a->lastModified() < b->lastModified();
                break;
              }
              return descending ? !less : less;
            });

  m_files = std::move(entries);

  for (int i = 0; i < m_files.size(); ++i) {
    const auto &entry = m_files[i];
    connect(entry.data(), &BaseDirectoryEntry::thumbnailReady, this,
            [this, i]() {
              const QModelIndex idx = index(i);
              emit dataChanged(idx, idx, {ThumbnailRole, Qt::DecorationRole});
            });
  }

  endResetModel();
}
