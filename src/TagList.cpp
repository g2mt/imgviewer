#include <imgviewer/Filter.h>
#include <imgviewer/TagList.h>

#include <QHeaderView>

TagList::TagList(Filter *filter, QWidget *parent)
    : QTreeWidget(parent), m_filter(filter) {
  setHeaderLabels({"", "Name"});
  header()->setStretchLastSection(true);
  header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  setRootIsDecorated(false);
  setSelectionMode(QAbstractItemView::MultiSelection);

  connect(m_filter, &Filter::tagsLoaded, this, &TagList::populate);
  connect(this, &QTreeWidget::itemSelectionChanged, this,
          &TagList::onSelectionChanged);
}

void TagList::populate() {
  clear();

  const auto &tagMap = m_filter->tagMap();
  for (auto it = tagMap.constBegin(); it != tagMap.constEnd(); ++it) {
    QTreeWidgetItem *item = new QTreeWidgetItem(this);
    item->setText(0, QString::number(it.value().size()));
    item->setText(1, it.key());
    item->setData(1, Qt::UserRole, it.key());
  }

  sortByColumn(1, Qt::AscendingOrder);
}

void TagList::onSelectionChanged() {
  QList<QString> list;
  for (auto *item : selectedItems())
    list.append(item->data(1, Qt::UserRole).toString());
  m_filter->setTags(list);
}
