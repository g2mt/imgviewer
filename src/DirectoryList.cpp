#include <imgviewer/DirectoryList.h>
#include <imgviewer/Filter.h>
#include <imgviewer/utils.h>

#include <QDir>
#include <QHeaderView>
#include <QIcon>

DirectoryList::DirectoryList(Filter *filter, QWidget *parent)
    : QTreeWidget(parent), m_filter(filter) {
  setHeaderLabels({"", "Name"});
  header()->setStretchLastSection(true);
  header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  setRootIsDecorated(false);
  setSelectionMode(QAbstractItemView::SingleSelection);

  populate();
  connect(m_filter, &Filter::changed, this, &DirectoryList::populate);
  connect(this, &QTreeWidget::itemActivated, this,
          &DirectoryList::onItemActivated);
}

void DirectoryList::populate() {
  clear();

  const QDir &current = m_filter->currentPath();
  QTreeWidgetItem *upItem = new QTreeWidgetItem(this);
  upItem->setIcon(0, QIcon::fromTheme("go-up"));
  upItem->setText(1, "..");
  upItem->setData(1, Qt::UserRole, "..");

  const auto entries =
      current.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const auto &info : entries) {
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setIcon(0, QIcon::fromTheme("folder"));
    item->setText(1, info.fileName());
    item->setData(1, Qt::UserRole, info.fileName());
  }

  const auto files = current.entryInfoList(QDir::Files, QDir::Name);
  for (const auto &info : files) {
    if (isArchivePath(info.absoluteFilePath())) {
      QTreeWidgetItem *item = new QTreeWidgetItem(this);
      item->setIcon(0, QIcon::fromTheme("application-x-archive",
                                        QIcon::fromTheme("package-x-generic")));
      item->setText(1, info.fileName());
      item->setData(1, Qt::UserRole, info.fileName());
    }
  }
}

void DirectoryList::onItemActivated(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  QString dirName = item->data(1, Qt::UserRole).toString();
  m_filter->navigateDirectory(dirName);
}
