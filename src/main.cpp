#include <QApplication>
#include <QCoreApplication>
#include <imgviewer/MainWindow.h>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("QtImgViewer");
  QCoreApplication::setApplicationName("ImageViewer");
  MainWindow win;
  win.resize(1000, 700);
  win.show();
  return app.exec();
}
