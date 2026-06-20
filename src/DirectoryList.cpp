#include <imgviewer/DirectoryList.h>
#include <imgviewer/Filter.h>

#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <qassert.h>

DirectoryList::DirectoryList(Filter *filter, QWidget *parent)
    : QTreeWidget(parent), m_filter(filter) {
  setHeaderLabels({"", "Name"});
  header()->setStretchLastSection(true);
  header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  setRootIsDecorated(false);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSortingEnabled(true);
  sortByColumn(1, Qt::AscendingOrder);

  populate();
  connect(m_filter, &Filter::changed, this, &DirectoryList::populate);
  connect(this, &QTreeWidget::itemActivated, this,
          &DirectoryList::onItemActivated);
}

void DirectoryList::populate() {
  clear();

  auto upEntry = QSharedPointer<DirectoryEntry>::create(
      QUrl(QStringLiteral("..")));
  QTreeWidgetItem *upItem = new QTreeWidgetItem(this);
  upItem->setIcon(0, QIcon::fromTheme("go-up"));
  upItem->setText(1, "..");
  upItem->setData(1, Qt::UserRole,
                  QVariant::fromValue(upEntry));

  const auto entries = m_filter->listDirectoryEntries();
  for (const auto &entry : entries) {
    if (entry->isDir() || entry->isArchivePath()) {
      QTreeWidgetItem *item = new QTreeWidgetItem(this);
      item->setIcon(
          0, entry->isDir()
                 ? QIcon::fromTheme("folder")
                 : QIcon::fromTheme("application-x-archive",
                                    QIcon::fromTheme("package-x-generic")));
      item->setText(1, entry->name());
      item->setData(1, Qt::UserRole, QVariant::fromValue(entry));
    }
  }
}

void DirectoryList::onItemActivated(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  QVariant data = item->data(1, Qt::UserRole);
  auto entry = data.value<QSharedPointer<DirectoryEntry>>();
  Q_ASSERT(entry);
  m_filter->navigateDirectory(*entry);
}
