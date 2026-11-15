#include <imgviewer/Filter.h>
#include <imgviewer/ImageDetailList.h>
#include <imgviewer/ImageDetailListContainer.h>

#include <QLineEdit>
#include <QVBoxLayout>

ImageDetailListContainer::ImageDetailListContainer(Filter *filter,
                                                   QWidget *parent)
    : QWidget(parent) {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_imageList = new ImageDetailList(filter);
  layout->addWidget(m_imageList, 1);

  m_filterInput = new QLineEdit();
  m_filterInput->setPlaceholderText("Search images...");
  m_filterInput->addAction(QIcon::fromTheme("edit-find"),
                           QLineEdit::LeadingPosition);
  m_filterInput->setClearButtonEnabled(true);
  layout->addWidget(m_filterInput);

  connect(m_filterInput, &QLineEdit::textChanged, filter,
          &Filter::setSearch);
}
