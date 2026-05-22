#pragma once
#include <QByteArray>
#include <QDateTime>
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
