#include <imgviewer/DirectoryList.h>
#include <imgviewer/Filter.h>

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

  QTreeWidgetItem *upItem = new QTreeWidgetItem(this);
  upItem->setIcon(0, QIcon::fromTheme("go-up"));
  upItem->setText(1, "..");
  upItem->setData(1, Qt::UserRole, "..");

  const auto entries = m_filter->listDirectoryEntries(m_filter->currentPath());
  for (const auto &entry : entries) {
    if (entry.isDir || entry.isArchivePath()) {
      QTreeWidgetItem *item = new QTreeWidgetItem(this);
      item->setIcon(0, entry.isDir ? QIcon::fromTheme("folder")
                                   : QIcon::fromTheme(
                                         "application-x-archive",
                                         QIcon::fromTheme("package-x-generic")));
      item->setText(1, entry.path.fileName());
      item->setData(1, Qt::UserRole, entry.path.toString());
    }
  }
}

void DirectoryList::onItemActivated(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  QString path = item->data(1, Qt::UserRole).toString();
  m_filter->navigateDirectory(path);
}
