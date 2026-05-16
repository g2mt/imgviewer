#pragma once
#include <QFrame>
#include <QLabel>
#include <QPixmap>
#include <QPoint>

class ImageView : public QFrame {
  Q_OBJECT

public:
  ImageView(QWidget *parent = nullptr);
  void setImage(const QString &path);

protected:
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void updateImageDisplay();
  bool hasImage() const;

  QLabel *m_placeholder;
  QPixmap m_pixmap;
  QPoint m_panOffset;
  QPoint m_lastMousePos;
  bool m_panning = false;
};
