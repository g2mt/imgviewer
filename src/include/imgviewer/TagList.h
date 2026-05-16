#pragma once
#include <QTreeWidget>

class Filter;

class TagList : public QTreeWidget {
  Q_OBJECT
public:
  TagList(Filter *filter, QWidget *parent = nullptr);

private slots:
  void populate();
  void onSelectionChanged();

private:
  Filter *m_filter;
};
