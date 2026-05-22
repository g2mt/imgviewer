#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

struct DirectoryEntry {
  QString name;
  QString path;
  bool isDir = false;
  QDateTime birthTime;
  QDateTime lastModified;
};

bool isImagePath(const QString &path);
bool isArchivePath(const QString &path);
QString childPath(const QString &basePath, const QString &childName);
QString parentPath(const QString &path);
QString archiveUrlForPath(const QString &path);
QList<DirectoryEntry> listDirectoryEntries(const QString &path);
QByteArray readFileBytes(const QString &path);
