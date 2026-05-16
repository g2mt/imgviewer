#pragma once
#include <QAbstractListModel>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSet>

class Filter;

// Lists image files in Filter's current directory, applying the search and
// sort settings. Thumbnails are decoded lazily on a worker thread and cached;
// requesting ThumbnailRole for an item kicks off a load if needed and returns
// an empty pixmap until the load completes (at which point dataChanged() is
// emitted for that index).
class ImageDetailModel : public QAbstractListModel {
  Q_OBJECT
public:
  static constexpr int kThumbnailSize = 96;

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
  void reload();
  // Invoked (queued) from the worker thread when a thumbnail is decoded.
  void onThumbnailReady(const QString &path, const QImage &image);

private:
  void requestThumbnail(const QString &path) const;

  Filter *m_filter;
  QList<QFileInfo> m_files;
  mutable QHash<QString, QPixmap> m_thumbnails;
  mutable QSet<QString> m_pending;
};
