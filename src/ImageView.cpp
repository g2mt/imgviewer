#include <imgviewer/ImageView.h>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>

ImageView::ImageView(QWidget *parent) : QFrame(parent) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_placeholder = new QLabel("Select an image...");
  m_placeholder->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_placeholder);

  setStyleSheet("background-color: #333; color: #eee;");
  setAcceptDrops(true);
}

void ImageView::setImage(const QString &path) {
  m_pixmap = QPixmap(path);
  m_panOffset = QPoint(0, 0);
  updateImageDisplay();
}

bool ImageView::hasImage() const { return !m_pixmap.isNull(); }

void ImageView::updateImageDisplay() {
  m_placeholder->setVisible(!hasImage());
  if (hasImage()) {
    m_placeholder->hide();
  }
  update();
}

void ImageView::paintEvent(QPaintEvent *event) {
  QFrame::paintEvent(event);
  if (!hasImage())
    return;

  QPainter painter(this);
  QPixmap scaled =
      m_pixmap.scaledToWidth(width(), Qt::SmoothTransformation);
  int x = m_panOffset.x();
  int y = (height() - scaled.height()) / 2 + m_panOffset.y();
  painter.drawPixmap(x, y, scaled);
}

void ImageView::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  if (hasImage())
    update();
}

void ImageView::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    for (const QUrl &url : event->mimeData()->urls()) {
      if (url.isLocalFile()) {
        QString path = url.toLocalFile();
        if (path.endsWith(".png", Qt::CaseInsensitive) ||
            path.endsWith(".jpg", Qt::CaseInsensitive) ||
            path.endsWith(".jpeg", Qt::CaseInsensitive) ||
            path.endsWith(".bmp", Qt::CaseInsensitive) ||
            path.endsWith(".gif", Qt::CaseInsensitive) ||
            path.endsWith(".webp", Qt::CaseInsensitive) ||
            path.endsWith(".tiff", Qt::CaseInsensitive) ||
            path.endsWith(".tif", Qt::CaseInsensitive)) {
          event->acceptProposedAction();
          return;
        }
      }
    }
  }
}

void ImageView::dragMoveEvent(QDragMoveEvent *event) {
  event->acceptProposedAction();
}

void ImageView::dropEvent(QDropEvent *event) {
  if (event->mimeData()->hasUrls()) {
    for (const QUrl &url : event->mimeData()->urls()) {
      if (url.isLocalFile()) {
        setImage(url.toLocalFile());
        return;
      }
    }
  }
}

void ImageView::mousePressEvent(QMouseEvent *event) {
  if (hasImage() && event->button() == Qt::LeftButton) {
    m_panning = true;
    m_lastMousePos = event->pos();
    setCursor(Qt::ClosedHandCursor);
  }
  QFrame::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent *event) {
  if (m_panning) {
    QPoint delta = event->pos() - m_lastMousePos;
    m_panOffset += delta;
    m_lastMousePos = event->pos();
    update();
  }
  QFrame::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_panning && event->button() == Qt::LeftButton) {
    m_panning = false;
    setCursor(Qt::ArrowCursor);
  }
  QFrame::mouseReleaseEvent(event);
}
