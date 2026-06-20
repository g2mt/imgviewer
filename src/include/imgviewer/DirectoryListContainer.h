#pragma once
#include <QWidget>

class DirectoryList;
class Filter;
class QLineEdit;

class DirectoryListContainer : public QWidget {
  Q_OBJECT
public:
  DirectoryListContainer(Filter *filter, QWidget *parent = nullptr);

private slots:
  void filterDirectory(const QString &text);

private:
  DirectoryList *m_dirList;
  QLineEdit *m_filterInput;
};
