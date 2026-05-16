#pragma once
#include "Filter.h"
#include <QMainWindow>
#include <QMenu>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>

class ImageView;

class MainWindow : public QMainWindow {
  Filter filter;

  void setupToolbar(QToolBar *toolbar);
  void setupFilterMenu(QMenu *filterMenu);
  void setupMainLayout(QVBoxLayout *mainLayout);
  ImageView *setupImageView(QSplitter *horizontalSplitter);
  void setupRightSplitter(QSplitter *horizontalSplitter, ImageView *imageView);

public:
  MainWindow();
};
