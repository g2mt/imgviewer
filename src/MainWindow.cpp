#include <QActionGroup>
#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <imgviewer/DirectoryListContainer.h>
#include <imgviewer/ImageDetailList.h>
#include <imgviewer/ImageView.h>
#include <imgviewer/MainWindow.h>
#include <imgviewer/TagList.h>
#include <imgviewer/TagListContainer.h>

MainWindow::MainWindow() {
  QSettings settings;
  QString imageDir = settings.value("image_directory", "").toString();
  if (!imageDir.isEmpty())
    filter.setCurrentPath(imageDir);

  QWidget *centralWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  setCentralWidget(centralWidget);

  m_toolbar = new QToolBar(this);
  addToolBar(m_toolbar);
  setupToolbar(m_toolbar);
  setupMainLayout(mainLayout);
  setupMenuBar();

  installEventFilter(this);
  setupEventFilters(this);

  QTimer::singleShot(0, [this]() {
    QSettings settings;
    QString tagsPath = settings.value("tags_path", "").toString();
    QString tagsPathReplace =
        settings.value("tags_path_replace", "").toString();
    if (!tagsPath.isEmpty())
      filter.loadTagsFile(tagsPath, tagsPathReplace);
  });
}

void MainWindow::setupFilterMenu(QMenu *filterMenu) {
  QActionGroup *sortGroup = new QActionGroup(this);
  sortGroup->setExclusive(true);

  auto makeSortAction = [&](const QString &text, SortBy sortBy,
                            bool checked = false) {
    QAction *act = filterMenu->addAction(text);
    act->setCheckable(true);
    act->setChecked(checked);
    act->setData(static_cast<int>(sortBy));
    sortGroup->addAction(act);
    return act;
  };
  makeSortAction("Name", SortBy::Name, true);
  makeSortAction("Date Created", SortBy::DateCreated);
  makeSortAction("Date Modified", SortBy::DateModified);

  filterMenu->addSeparator();
  QAction *descAction = filterMenu->addAction("Descending");
  descAction->setCheckable(true);
  QAction *natAction = filterMenu->addAction("Natural Sort");
  natAction->setCheckable(true);

  auto applySort = [this, descAction, natAction]() {
    filter.setDescending(descAction->isChecked());
    filter.setNaturalSort(natAction->isChecked());
  };

  connect(sortGroup, &QActionGroup::triggered, this,
          [this, applySort](QAction *action) {
            filter.setSortBy(static_cast<SortBy>(action->data().toInt()));
            applySort();
          });
  connect(descAction, &QAction::toggled, this,
          [applySort](bool) { applySort(); });
  connect(natAction, &QAction::toggled, this,
          [applySort](bool) { applySort(); });
}

void MainWindow::setupToolbar(QToolBar *toolbar) {
  m_backAction = toolbar->addAction(QIcon::fromTheme("go-previous"), "Back");
  m_forwardAction = toolbar->addAction(QIcon::fromTheme("go-next"), "Forward");
  m_backAction->setEnabled(false);
  m_forwardAction->setEnabled(false);

  QWidget *spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);

  QLineEdit *searchBox = new QLineEdit();
  searchBox->setPlaceholderText("Filter images");
  searchBox->addAction(QIcon::fromTheme("edit-find"),
                       QLineEdit::LeadingPosition);
  searchBox->setMaximumWidth(200);
  toolbar->addWidget(searchBox);
  connect(searchBox, &QLineEdit::textChanged,
          [this](const QString &text) { filter.setSearch(text); });

  QMenu *filterMenu = new QMenu("Filter", this);
  setupFilterMenu(filterMenu);

  QToolButton *filterBtn = new QToolButton();
  filterBtn->setIcon(QIcon::fromTheme("view-filter"));
  filterBtn->setPopupMode(QToolButton::InstantPopup);
  filterBtn->setMenu(filterMenu);
  toolbar->addWidget(filterBtn);
}

void MainWindow::setupMenuBar() {
  m_menuBar = new QMenuBar(this);
  setMenuBar(m_menuBar);

  QMenu *fileMenu = m_menuBar->addMenu("&File");
  QAction *quitAction = fileMenu->addAction("&Quit");
  quitAction->setShortcut(QKeySequence("Ctrl+Q"));
  connect(quitAction, &QAction::triggered, this, &QMainWindow::close);

  QMenu *viewMenu = m_menuBar->addMenu("&View");
  QAction *fullScreenAction = viewMenu->addAction("&Full Screen");
  fullScreenAction->setShortcut(QKeySequence("F11"));
  connect(fullScreenAction, &QAction::triggered, this, [this]() {
    if (isFullScreen())
      showNormal();
    else
      showFullScreen();
  });

  m_collapseViewAction = viewMenu->addAction("&Collapse View");
  m_collapseViewAction->setShortcut(QKeySequence("Tab"));
  m_collapseViewAction->setCheckable(true);
  connect(m_collapseViewAction, &QAction::triggered, this,
          &MainWindow::toggleCollapseView);

  viewMenu->addSeparator();
  QAction *flipHAction = viewMenu->addAction("Flip &Horizontal");
  flipHAction->setShortcut(QKeySequence("m"));
  flipHAction->setCheckable(true);
  connect(flipHAction, &QAction::triggered, m_imageView,
          &ImageView::setFlipHorizontal);
  QAction *flipVAction = viewMenu->addAction("Flip &Vertical");
  flipVAction->setShortcut(QKeySequence("v"));
  flipVAction->setCheckable(true);
  connect(flipVAction, &QAction::triggered, m_imageView,
          &ImageView::setFlipVertical);

  QMenu *filterMenu = m_menuBar->addMenu("Fil&ter");
  setupFilterMenu(filterMenu);
}

void MainWindow::toggleCollapseView() {
  bool collapsed = m_collapseViewAction->isChecked();
  m_toolbar->setVisible(!collapsed);
  m_menuBar->setVisible(!collapsed);
  m_rightSplitter->setVisible(!collapsed);
}

void MainWindow::setupMainLayout(QVBoxLayout *mainLayout) {
  m_horizontalSplitter = new QSplitter(Qt::Horizontal);
  mainLayout->addWidget(m_horizontalSplitter);

  setupImageView(m_horizontalSplitter);
  setupRightSplitter(m_horizontalSplitter);
}

void MainWindow::setupImageView(QSplitter *horizontalSplitter) {
  m_imageView = new ImageView(&filter);
  horizontalSplitter->addWidget(m_imageView);
  horizontalSplitter->setStretchFactor(0, 6);
  horizontalSplitter->setCollapsible(0, false);
}

void MainWindow::setupRightSplitter(QSplitter *horizontalSplitter) {
  m_rightSplitter = new QSplitter(Qt::Vertical);

  QTabWidget *tabs = new QTabWidget();

  DirectoryListContainer *dirList = new DirectoryListContainer(&filter);
  tabs->addTab(dirList, "Directory");
  TagListContainer *tagList = new TagListContainer(&filter);
  tabs->addTab(tagList, "Tags");
  m_rightSplitter->addWidget(tabs);

  ImageDetailList *imageList = new ImageDetailList(&filter);
  m_imageList = imageList;
  m_rightSplitter->addWidget(imageList);
  connect(imageList, &ImageDetailList::imageActivated, m_imageView,
          [this](const QVariant &data) {
#ifdef USE_QT_PDF
            if (auto *pdfEntry = data.value<PdfDirectoryEntry *>())
              m_imageView->setImage(pdfEntry->renderPage());
            else
#endif
              m_imageView->setImage(QUrl(data.toString()));
          });
  connect(m_imageView, &ImageView::goForward, imageList,
          &ImageDetailList::forward);
  connect(m_imageView, &ImageView::goBackward, imageList,
          &ImageDetailList::backward);
  connect(m_backAction, &QAction::triggered, imageList,
          &ImageDetailList::backward);
  connect(m_forwardAction, &QAction::triggered, imageList,
          &ImageDetailList::forward);
  connect(imageList->selectionModel(), &QItemSelectionModel::currentChanged,
          this, &MainWindow::updateNavButtons);
  connect(imageList->model(), &QAbstractItemModel::modelReset, this,
          &MainWindow::updateNavButtons);
  updateNavButtons();

  horizontalSplitter->addWidget(m_rightSplitter);
  horizontalSplitter->setStretchFactor(1, 1);
  horizontalSplitter->setCollapsible(1, false);
}

void MainWindow::updateNavButtons() {
  const QModelIndex current = m_imageList->currentIndex();
  if (!current.isValid()) {
    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);
    return;
  }
  m_backAction->setEnabled(current.row() > 0);
  m_forwardAction->setEnabled(current.row() <
                              m_imageList->model()->rowCount() - 1);
}

void MainWindow::setupEventFilters(QObject *obj) {
  for (QObject *child : obj->children()) {
    if (child->isWidgetType()) {
      child->installEventFilter(this);
    }
    setupEventFilters(child);
  }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if (keyEvent->key() == Qt::Key_Tab) {
      m_collapseViewAction->trigger();
      return false;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}
