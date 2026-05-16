#pragma once
#include <QTreeWidget>

class Filter;

class DirectoryList : public QTreeWidget {
  Q_OBJECT
public:
  // Takes a non-owning pointer to Filter. The Filter must outlive this
  // DirectoryList (both are owned by MainWindow).
  DirectoryList(Filter *filter, QWidget *parent = nullptr);

private slots:
  void populate();
  void onItemActivated(QTreeWidgetItem *item, int column);

private:
  Filter *m_filter;
};
