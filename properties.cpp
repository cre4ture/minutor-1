/** Copyright 2014 Rian Shelley */
#include <QRegularExpression>
#include <QVector3D>
#include "./properties.h"
#include "./ui_properties.h"


Properties::Properties(QWidget *parent) : QDialog(parent),
    ui(new Ui::Properties) {
  ui->setupUi(this);
}

Properties::~Properties() {
  delete ui;
}

void Properties::DisplayProperties(QVariant p) {
  // get current property
  QString propertyName;
  QTreeWidgetItem* item = ui->propertyView->currentItem();
  if (item) {
    propertyName = item->data(0, Qt::DisplayRole).toString();
  }

  ui->propertyView->clear();

  // only support QVariantMap or QVariantHash at this level
  switch (p.type()) {
    case QMetaType::QVariantMap:
      treeCreator.ParseIterable(ui->propertyView->invisibleRootItem(), p.toMap());
      break;
    case QMetaType::QVariantHash:
      treeCreator.ParseIterable(ui->propertyView->invisibleRootItem(), p.toHash());
      break;
    case QMetaType::QVariantList:
      treeCreator.ParseList(ui->propertyView->invisibleRootItem(), p.toList());
      break;
    default:
      qWarning("Trying to display scalar value as a property");
      break;
  }

  // expand at least the first level
  ui->propertyView->expandToDepth(0);

  if (propertyName.size() != 0) {
    // try to restore the path
    QList<QTreeWidgetItem*> items =
        ui->propertyView->findItems(propertyName, Qt::MatchRecursive);
    if (items.size())
      items.front()->setSelected(true);
  }
}

void Properties::on_propertyView_itemClicked(QTreeWidgetItem *item, int column)
{
  if (evaluateBB(item))
  {
    return;
  }

  for (int i = 0; i < item->childCount(); i++)
  {
    if (evaluateBB(item->child(i)))
    {
      return;
    }
  }
}

bool Properties::evaluateBB(QTreeWidgetItem *item)
{
  QString text = item->data(0, Qt::DisplayRole).toString();

  if ((text == "BB") && (item->childCount() >= 6))
  {
    QVector3D from;
    from.setX(item->child(0)->data(1, Qt::DisplayRole).toFloat());
    from.setY(item->child(1)->data(1, Qt::DisplayRole).toFloat());
    from.setZ(item->child(2)->data(1, Qt::DisplayRole).toFloat());

    QVector3D to;
    to.setX(item->child(3)->data(1, Qt::DisplayRole).toFloat());
    to.setY(item->child(4)->data(1, Qt::DisplayRole).toFloat());
    to.setZ(item->child(5)->data(1, Qt::DisplayRole).toFloat());

    emit onBoundingBoxSelected(from, to);
    return true;
  }

  return false;
}

void Properties::on_propertyView_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    on_propertyView_itemClicked(current, 0);
}
