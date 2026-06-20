#pragma once
#include <QDateTime>
#include <QImage>
#include <QMetaType>
#include <QUrl>
#include <variant>

struct VirtualDirectoryEntry {
  int index = 0;
  QImage image;
};

struct DirectoryEntry {
  std::variant<QUrl, VirtualDirectoryEntry> data;
  bool isDir = false;
  QDateTime birthTime;
  QDateTime lastModified;

  bool isImagePath() const;
  bool isArchivePath() const;

  QString name() const {
    if (auto *url = std::get_if<QUrl>(&data))
      return url->fileName();
    return QStringLiteral("Page %1").arg(
        std::get<VirtualDirectoryEntry>(data).index + 1, 2, 10,
        QLatin1Char('0'));
  }
};

Q_DECLARE_METATYPE(DirectoryEntry)
