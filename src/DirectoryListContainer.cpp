#include <imgviewer/DirectoryList.h>
#include <imgviewer/DirectoryListContainer.h>

#include <QLineEdit>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

DirectoryListContainer::DirectoryListContainer(Filter *filter, QWidget *parent)
    : QWidget(parent) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_dirList = new DirectoryList(filter);
  layout->addWidget(m_dirList, 1);

  m_filterInput = new QLineEdit();
  m_filterInput->setPlaceholderText("Search directory...");
  m_filterInput->setClearButtonEnabled(true);
  layout->addWidget(m_filterInput);

  connect(m_filterInput, &QLineEdit::textChanged, this,
          &DirectoryListContainer::filterDirectory);
}

void DirectoryListContainer::filterDirectory(const QString &text) {
  for (int i = 0; i < m_dirList->topLevelItemCount(); ++i) {
    QTreeWidgetItem *item = m_dirList->topLevelItem(i);
    bool visible =
        text.isEmpty() || item->text(1).contains(text, Qt::CaseInsensitive);
    item->setHidden(!visible);
  }
}
