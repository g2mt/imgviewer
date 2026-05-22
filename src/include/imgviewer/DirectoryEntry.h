#pragma once
#include <QDateTime>
#include <QMetaType>
#include <QUrl>

struct DirectoryEntry {
  QUrl path;
  bool isDir = false;
  QDateTime birthTime;
  QDateTime lastModified;

  bool isImagePath() const;
  bool isArchivePath() const;
};

Q_DECLARE_METATYPE(DirectoryEntry)
