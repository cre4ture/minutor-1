/** Copyright 2014 Rian Shelley */
#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include "propertietreecreator.h"

#include <QDialog>
#include <QMap>
#include <QVector3D>

namespace Ui {
class Properties;
}

class QTreeWidgetItem;

class Properties : public QDialog {
  Q_OBJECT

 public:
  explicit Properties(QWidget *parent = 0);
  ~Properties();

  void DisplayProperties(QVariant p);

signals:
  void onBoundingBoxSelected(QVector3D from, QVector3D to);

private slots:
  void on_propertyView_itemClicked(QTreeWidgetItem *item, int column);

  void on_propertyView_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

private:
  Ui::Properties *ui;
  PropertieTreeCreator m_creator;

  bool evaluateBB(QTreeWidgetItem *item);
};

#endif  // PROPERTIES_H_
