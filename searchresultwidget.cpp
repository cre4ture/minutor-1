#include "searchresultwidget.h"
#include "ui_searchresultwidget.h"

#include "properties.h"

Q_DECLARE_METATYPE(SearchResultItem);

SearchResultWidget::SearchResultWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SearchResultWidget)
{
    ui->setupUi(this);
}

SearchResultWidget::~SearchResultWidget()
{
    delete ui;
}

void SearchResultWidget::clearResults()
{
    ui->treeWidget->clear();
}

void SearchResultWidget::addResult(const SearchResultItem &result)
{
    auto text = QString("%1")
            .arg(result.name)
            ;

    auto item = new QTreeWidgetItem();
    item->setData(0, Qt::UserRole, QVariant::fromValue(result));
    item->setText(0, text);
    item->setText(1, result.buys);
    item->setText(2, result.sells);

    ui->treeWidget->addTopLevelItem(item);
}

void SearchResultWidget::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!m_properties)
    {
        m_properties = QSharedPointer<Properties>::create();
    }

    auto props = item->data(0, Qt::UserRole).value<SearchResultItem>().properties;
    m_properties->DisplayProperties(props);
    m_properties->showNormal();
}

void SearchResultWidget::on_treeWidget_itemSelectionChanged()
{
    auto list = ui->treeWidget->selectedItems();
    if (list.size() > 0)
    {
        auto item = list[0];
        auto data = item->data(0, Qt::UserRole).value<SearchResultItem>();
        emit jumpTo(data.pos);
    }
}
