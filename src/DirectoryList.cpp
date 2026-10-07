#include "imgviewer/DirectoryEntry.h"
#include <imgviewer/DirectoryList.h>
#include <imgviewer/Filter.h>

#include <QDir>
#include <QHeaderView>
#include <QIcon>
#include <qassert.h>
#include <qsharedpointer.h>

namespace {

enum { TypeRole = Qt::UserRole, DateRole = Qt::UserRole + 1 };

class DirTreeItem : public QTreeWidgetItem {
public:
  using QTreeWidgetItem::QTreeWidgetItem;

private:
  bool operator<(const QTreeWidgetItem &other) const override {
    int col = treeWidget() ? treeWidget()->sortColumn() : 0;
    if (col == 0) {
      int a = data(0, TypeRole).toInt();
      int b = other.data(0, TypeRole).toInt();
      if (a != b)
        return a < b;
      return text(1).compare(other.text(1), Qt::CaseInsensitive) < 0;
    }
    if (col == 2 || col == 3)
      return data(col, DateRole).toDateTime() <
             other.data(col, DateRole).toDateTime();
    return QTreeWidgetItem::operator<(other);
  }
};

} // namespace

DirectoryList::DirectoryList(Filter *filter, QWidget *parent)
    : QTreeWidget(parent), m_filter(filter) {
  setHeaderLabels({"", "Name", "Created", "Modified"});
  header()->setStretchLastSection(true);
  header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  setRootIsDecorated(false);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setSortingEnabled(true);
  sortByColumn(1, Qt::AscendingOrder);

  populate();
  connect(m_filter, &Filter::dirEntriesLoaded, this, &DirectoryList::populate);
  connect(this, &QTreeWidget::itemActivated, this,
          &DirectoryList::onItemActivated);
}

void DirectoryList::populate() {
  clear();

  auto upEntry = QSharedPointer<DirectoryEntry>(UpDirectoryEntry::create());
  QTreeWidgetItem *upItem = new DirTreeItem(this);
  upItem->setIcon(0, QIcon::fromTheme("go-up"));
  upItem->setText(1, "..");
  upItem->setData(0, TypeRole, static_cast<int>(BaseDirectoryEntry::EntryType::Up));
  upItem->setData(1, Qt::UserRole, QVariant::fromValue(upEntry));

  const auto &entries = m_filter->dirEntries();
  for (const auto &base_entry : entries) {
    if (base_entry->entryType() == BaseDirectoryEntry::EntryType::Dir ||
        base_entry->entryType() == DirectoryEntry::EntryType::Archive ||
        base_entry->entryType() == DirectoryEntry::EntryType::Pdf) {
      QSharedPointer<DirectoryEntry> entry =
          qSharedPointerCast<DirectoryEntry>(base_entry);
      QTreeWidgetItem *item = new DirTreeItem(this);
      item->setData(0, TypeRole, static_cast<int>(base_entry->entryType()));
      QIcon icon;
      if (base_entry->entryType() == BaseDirectoryEntry::EntryType::Dir)
        icon = QIcon::fromTheme("folder");
      else if (base_entry->entryType() ==
               BaseDirectoryEntry::EntryType::Pdf)
        icon = QIcon::fromTheme("application-pdf");
      else
        icon = QIcon::fromTheme("application-x-archive",
                                QIcon::fromTheme("package-x-generic"));
      item->setIcon(0, icon);
      item->setText(1, base_entry->name());
      item->setText(2, entry->birthTime().toString(Qt::ISODate));
      item->setText(3, entry->lastModified().toString(Qt::ISODate));
      item->setData(2, DateRole, entry->birthTime());
      item->setData(3, DateRole, entry->lastModified());
      item->setData(1, Qt::UserRole, QVariant::fromValue(entry));
    }
  }
}

void DirectoryList::onItemActivated(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  QVariant data = item->data(1, Qt::UserRole);
  QSharedPointer<DirectoryEntry> entry =
      data.value<QSharedPointer<DirectoryEntry>>();
  Q_ASSERT(entry);
  m_filter->navigateDirectory(entry);
}
