#pragma once
#include <QByteArray>
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
  QByteArray readFileBytes() const;
};

Q_DECLARE_METATYPE(DirectoryEntry)
