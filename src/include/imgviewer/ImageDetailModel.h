#pragma once
#include <QAbstractListModel>
#include <QImage>
#include <QPixmap>

#include <imgviewer/Filter.h>

class Filter;

// Lists image files in Filter's current directory, applying the search and
// sort settings. Thumbnails are decoded lazily by each entry and cached;
// requesting ThumbnailRole for an item kicks off a load if needed and returns
// an empty pixmap until the load completes (at which point dataChanged() is
// emitted for that index).
class ImageDetailModel : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles {
    FilePathRole = Qt::UserRole + 1,
    FileNameRole,
    ThumbnailRole,
  };

  ImageDetailModel(Filter *filter, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

private slots:
  void populate();

private:
  Filter *m_filter;
  QList<int> m_filteredIndices;
};
