#ifdef USE_KIO
#include <KIO/StoredTransferJob>
#include <KJob>
#endif
#include <QBuffer>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGestureEvent>
#include <QImageReader>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPinchGesture>
#include <QResizeEvent>
#include <QTransform>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <imgviewer/Filter.h>
#include <imgviewer/ImageView.h>
#include <qevent.h>

namespace {
// Multiplicative zoom factor applied per wheel "notch" (120 eighths of a
// degree, the standard step on most mice).
constexpr float kZoomStepPerNotch = 1.15f;
constexpr float kMinZoom = 0.01f;
constexpr float kMaxZoom = 100.0f;
} // namespace

ImageView::ImageView(Filter *filter, QWidget *parent)
    : QFrame(parent), m_filter(filter) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_placeholder = new QLabel("Select an image...");
  m_placeholder->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_placeholder);

  setStyleSheet("background-color: #333; color: #eee;");
  setAcceptDrops(true);
  setMouseTracking(true);
  grabGesture(Qt::PinchGesture);

  m_cursorTimer = new QTimer(this);
  m_cursorTimer->setSingleShot(true);
  m_cursorTimer->setInterval(2000);
  connect(m_cursorTimer, &QTimer::timeout, this, [this]() {
    if (!m_panning) {
      setCursor(Qt::BlankCursor);
      m_cursorHidden = true;
    }
  });
}

void ImageView::setImage(const QUrl &url) {
  m_originalPixmap = {};
  m_pixmap = {};

#ifdef USE_LIBARCHIVE
  if (!url.isLocalFile())
    return;
  QImageReader reader(url.toLocalFile());
  reader.setAutoTransform(true);
  m_originalPixmap = QPixmap::fromImage(reader.read());
  applyFlip();
  resetCamera();
  updateImageDisplay();
#elif defined(USE_KIO)
  // Cancel any in-flight KIO request
  if (m_currentJob) {
    KIO::StoredTransferJob *oldJob = m_currentJob;
    m_currentJob = nullptr;
    oldJob->kill();
  }

  KIO::StoredTransferJob *job =
      KIO::storedGet(url, KIO::NoReload, KIO::HideProgressInfo);
  connect(job, &KIO::StoredTransferJob::result, this, [this, job]() {
    if (m_currentJob != job)
      return;
    m_currentJob = nullptr;

    if (job->error())
      return;

    const QByteArray bytes = job->data();
    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    m_originalPixmap = QPixmap::fromImage(reader.read());
    applyFlip();
    resetCamera();
    updateImageDisplay();
  });
  m_currentJob = job;
#endif
}

void ImageView::setImage(const QImage &image) {
  m_originalPixmap = QPixmap::fromImage(image);
  m_pixmap = {};
  applyFlip();
  resetCamera();
  updateImageDisplay();
}

void ImageView::applyFlip() {
  QTransform t;
  if (m_flipH)
    t.scale(-1, 1);
  if (m_flipV)
    t.scale(1, -1);
  m_pixmap =
      t.isIdentity() ? m_originalPixmap : m_originalPixmap.transformed(t);
}

bool ImageView::hasImage() const { return !m_pixmap.isNull(); }

void ImageView::resetCamera() {
  if (!hasImage()) {
    m_camera = Camera{};
    return;
  }

  // Fit the image to the widget width and center it vertically. The image's
  // top-left corner (0, 0) is taken as the camera target so `offset` is the
  // on-screen position of that corner.
  const float fitZoom =
      m_pixmap.width() > 0 ? float(width()) / float(m_pixmap.width()) : 1.0f;
  m_camera.zoom = fitZoom > 0.0f ? fitZoom : 1.0f;
  m_camera.imageTarget = QPointF(0.0, 0.0);
  m_camera.offset = QPointF(0.0, 0.0);
}

void ImageView::updateImageDisplay() {
  m_placeholder->setVisible(!hasImage());
  if (hasImage()) {
    m_placeholder->hide();
  }
  update();
}

void ImageView::setFlipHorizontal(bool flip) {
  if (m_flipH == flip)
    return;
  m_flipH = flip;
  applyFlip();
  update();
}

void ImageView::setFlipVertical(bool flip) {
  if (m_flipV == flip)
    return;
  m_flipV = flip;
  applyFlip();
  update();
}

void ImageView::paintEvent(QPaintEvent *event) {
  QFrame::paintEvent(event);
  if (!hasImage())
    return;

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  // The destination rectangle is the image's bounds mapped through the camera.
  const QPointF topLeft = m_camera.imageToScreen(QPointF(0.0, 0.0));
  const QSizeF dstSize(m_pixmap.width() * m_camera.zoom,
                       m_pixmap.height() * m_camera.zoom);
  painter.drawPixmap(QRectF(topLeft, dstSize), m_pixmap,
                     QRectF(m_pixmap.rect()));
}

void ImageView::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  if (hasImage())
    update();
}

bool ImageView::event(QEvent *event) {
  if (event->type() == QEvent::NativeGesture) {
    auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
    if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
      const QPointF cursor = gestureEvent->position();
      const QPointF imagePointUnderCursor = m_camera.screenToImage(cursor);
      m_camera.zoom = std::clamp(m_camera.zoom + (float)(gestureEvent->value()),
                                 kMinZoom, kMaxZoom);
      m_camera.imageTarget = imagePointUnderCursor;
      m_camera.offset = cursor;
      update();
      return true;
    }
  }
  return QFrame::event(event);
}

void ImageView::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    for (const QUrl &url : event->mimeData()->urls()) {
      if (url.isLocalFile()) {
        QString path = url.toLocalFile();
        auto entry =
            QSharedPointer<DirectoryEntry>::create(QUrl::fromLocalFile(path));
        if (entry->isImagePath()) {
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
  m_cursorTimer->start();
  if (m_cursorHidden) {
    setCursor(Qt::ArrowCursor);
    m_cursorHidden = false;
  }
  if (hasImage() && event->button() == Qt::LeftButton) {
    m_panning = true;
    m_lastMousePos = event->position();
    setCursor(Qt::ClosedHandCursor);
  }
  QFrame::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent *event) {
  if (m_cursorHidden) {
    setCursor(m_panning ? Qt::ClosedHandCursor : Qt::ArrowCursor);
    m_cursorHidden = false;
  }
  m_cursorTimer->start();
  if (m_panning) {
    const QPointF delta = event->position() - m_lastMousePos;
    m_camera.offset += delta;
    m_lastMousePos = event->position();
    update();
  }
  QFrame::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_panning && event->button() == Qt::LeftButton) {
    m_panning = false;
    setCursor(Qt::ArrowCursor);
    m_cursorHidden = false;
    m_cursorTimer->start();
  }
  QFrame::mouseReleaseEvent(event);
}

void ImageView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (event->position().x() < width() / 2.0)
      emit goBackward();
    else
      emit goForward();
  }
  QFrame::mouseDoubleClickEvent(event);
}

void ImageView::wheelEvent(QWheelEvent *event) {
  if (!hasImage()) {
    QFrame::wheelEvent(event);
    return;
  }

  if (event->modifiers() & Qt::ControlModifier) {
    const QPointF cursor = event->position();
    const QPointF imagePointUnderCursor = m_camera.screenToImage(cursor);

    const float notches = event->angleDelta().y() / 120.0f;
    const float factor = std::pow(kZoomStepPerNotch, notches);
    m_camera.zoom = std::clamp(m_camera.zoom * factor, kMinZoom, kMaxZoom);

    m_camera.imageTarget = imagePointUnderCursor;
    m_camera.offset = cursor;
  } else {
    const QPointF delta = event->angleDelta() / 8.0;
    m_camera.offset += delta;
  }

  update();
  event->accept();
}
