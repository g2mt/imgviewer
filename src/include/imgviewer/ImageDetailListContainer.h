#pragma once
#include <QWidget>

class Filter;
class ImageDetailList;
class QLineEdit;

class ImageDetailListContainer : public QWidget {
public:
  ImageDetailListContainer(Filter *filter, QWidget *parent = nullptr);

  ImageDetailList *imageList() const { return m_imageList; }

private:
  ImageDetailList *m_imageList;
  QLineEdit *m_filterInput;
};
