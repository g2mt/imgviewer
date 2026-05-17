#pragma once
#include <QString>

bool isImagePath(const QString &path);
bool isArchivePath(const QString &path);
bool extractArchiveTo(const QString &archivePath, const QString &destDir);
