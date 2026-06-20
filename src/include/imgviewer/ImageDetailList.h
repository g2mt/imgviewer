#pragma once
#include <QListView>
#include <QVariant>

class Filter;
class ImageDetailModel;

class ImageDetailList : public QListView {
  Q_OBJECT
public:
  // Takes a non-owning pointer to Filter. The Filter must outlive this
  // ImageDetailList (both are owned by MainWindow).
  ImageDetailList(Filter *filter, QWidget *parent = nullptr);

public slots:
  void forward();
  void backward();

signals:
  // Emitted when the user activates an item (single click). Carries the
  // absolute path of the selected image file.
  void imageActivated(const QVariant &data);

private slots:
  void onClicked(const QModelIndex &index);

private:
  ImageDetailModel *m_model;
};
