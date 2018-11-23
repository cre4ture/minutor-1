#include "searchresultwidget.h"
#include "ui_searchresultwidget.h"

#include "properties.h"

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
    item->setData(0, Qt::UserRole, result.properties);
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

    auto props = item->data(0, Qt::UserRole);
    m_properties->DisplayProperties(props);
    m_properties->showNormal();
}
