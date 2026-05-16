#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <imgviewer/DirectoryList.h>
#include <imgviewer/ImageDetailList.h>
#include <imgviewer/ImageView.h>
#include <imgviewer/MainWindow.h>
#include <imgviewer/TagList.h>

MainWindow::MainWindow() {
  QWidget *centralWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  setCentralWidget(centralWidget);

  QToolBar *toolbar = new QToolBar(this);
  addToolBar(toolbar);
  setupToolbar(toolbar);
  setupMainLayout(mainLayout);
}

void MainWindow::setupFilterMenu(QMenu *filterMenu) {
  QAction *nameAct = filterMenu->addAction("Name");
  nameAct->setCheckable(true);
  nameAct->setChecked(true);
  QAction *dateCreatedAct = filterMenu->addAction("Date Created");
  dateCreatedAct->setCheckable(true);
  QAction *dateModifiedAct = filterMenu->addAction("Date Modified");
  dateModifiedAct->setCheckable(true);
  filterMenu->addSeparator();
  QAction *descAction = filterMenu->addAction("Descending");
  descAction->setCheckable(true);
  QAction *natAction = filterMenu->addAction("Natural Sort");
  natAction->setCheckable(true);

  connect(nameAct, &QAction::triggered,
          [this]() { filter.setSortBy(SortBy::Name); });
  connect(dateCreatedAct, &QAction::triggered,
          [this]() { filter.setSortBy(SortBy::DateCreated); });
  connect(dateModifiedAct, &QAction::triggered,
          [this]() { filter.setSortBy(SortBy::DateModified); });
  connect(descAction, &QAction::toggled,
          [this](bool checked) { filter.setDescending(checked); });
  connect(natAction, &QAction::toggled,
          [this](bool checked) { filter.setNaturalSort(checked); });
}

void MainWindow::setupToolbar(QToolBar *toolbar) {
  toolbar->addAction(QIcon::fromTheme("go-previous"), "Back");
  toolbar->addAction(QIcon::fromTheme("go-next"), "Forward");

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

void MainWindow::setupMainLayout(QVBoxLayout *mainLayout) {
  QSplitter *horizontalSplitter = new QSplitter(Qt::Horizontal);
  mainLayout->addWidget(horizontalSplitter);

  ImageView *imageView = setupImageView(horizontalSplitter);
  setupRightSplitter(horizontalSplitter, imageView);
}

ImageView *MainWindow::setupImageView(QSplitter *horizontalSplitter) {
  ImageView *imageView = new ImageView();
  horizontalSplitter->addWidget(imageView);
  horizontalSplitter->setStretchFactor(0, 6);
  horizontalSplitter->setCollapsible(0, false);
  return imageView;
}

void MainWindow::setupRightSplitter(QSplitter *horizontalSplitter,
                                    ImageView *imageView) {
  QSplitter *rightSplitter = new QSplitter(Qt::Vertical);

  QTabWidget *tabs = new QTabWidget();
  DirectoryList *dirList = new DirectoryList(&filter);
  tabs->addTab(dirList, "Directory");
  TagList *tagList = new TagList();
  tabs->addTab(tagList, "Tags");
  rightSplitter->addWidget(tabs);
  connect(tagList, &TagList::itemSelectionChanged, [this, tagList]() {
    QList<QString> list;
    for (auto item : tagList->selectedItems())
      list.append(item->text());
    filter.setTags(list);
  });

  ImageDetailList *imageList = new ImageDetailList(&filter);
  rightSplitter->addWidget(imageList);
  connect(imageList, &ImageDetailList::imageActivated, imageView,
          &ImageView::setImage);

  horizontalSplitter->addWidget(rightSplitter);
  horizontalSplitter->setStretchFactor(1, 1);
  horizontalSplitter->setCollapsible(1, false);
}
