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

class MyTreeWidgetItem : public QTreeWidgetItem {
  public:
  MyTreeWidgetItem(QTreeWidget* parent):QTreeWidgetItem(parent){}
  private:
  bool operator<(const QTreeWidgetItem &other)const {
     int column = treeWidget()->sortColumn();
     switch (column)
     {
     case 1:
         return text(column).toDouble() < other.text(column).toDouble();
     default:
         return QTreeWidgetItem::operator<(other);
     }
  }
};

void SearchResultWidget::addResult(const SearchResultItem &result)
{
    auto text = QString("%1")
            .arg(result.name)
            ;

    auto item = new MyTreeWidgetItem(nullptr);
    item->setData(0, Qt::UserRole, QVariant::fromValue(result));

    QVector3D distance = m_pointOfInterest - result.pos;
    float airDistance = distance.length();

    int c = 0;
    item->setText(c++, text);
    item->setText(c++, QString::number(std::roundf(airDistance)));
    item->setText(c++, QString("%1,%2,%3").arg(result.pos.x()).arg(result.pos.y()).arg(result.pos.z()));
    item->setText(c++, result.buys);
    item->setText(c++, result.sells);

    ui->treeWidget->addTopLevelItem(item);
}

void SearchResultWidget::setPointOfInterest(const QVector3D &centerPoint)
{
    m_pointOfInterest = centerPoint;
    ui->lbl_location->setText(QString("Results around position: %1,%2,%3").arg(m_pointOfInterest.x()).arg(m_pointOfInterest.y()).arg(m_pointOfInterest.z()));
}

void SearchResultWidget::on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    auto properties = new Properties();

    auto props = item->data(0, Qt::UserRole).value<SearchResultItem>().properties;
    properties->DisplayProperties(props);
    properties->showNormal();
}

void SearchResultWidget::on_treeWidget_itemSelectionChanged()
{
    auto list = ui->treeWidget->selectedItems();
    if (list.size() > 0)
    {
        auto item = list[0];
        auto data = item->data(0, Qt::UserRole).value<SearchResultItem>();
        emit jumpTo(data.pos);
        emit highlightEntity(data.entity);
    }
}

void SearchResultWidget::on_treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    on_treeWidget_itemSelectionChanged();
}
