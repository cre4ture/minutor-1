#ifndef SEARCHRESULTWIDGET_H
#define SEARCHRESULTWIDGET_H

#include "properties.h"

#include <QWidget>
#include <QVariant>
#include <QVector3D>

class QTreeWidgetItem;

namespace Ui {
class SearchResultWidget;
}

class SearchResultItem
{
public:
    QString name;
    QVector3D pos;
    QString buys;
    QString sells;
    QVariant properties;
};

class SearchResultWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchResultWidget(QWidget *parent = nullptr);
    ~SearchResultWidget();

    void clearResults();
    void addResult(const SearchResultItem &result);

    void setPointOfInterest(const QVector3D& centerPoint);

signals:
    void jumpTo(QVector3D pos);

protected slots:
    void on_treeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);
private slots:
    void on_treeWidget_itemSelectionChanged();

private:
    Ui::SearchResultWidget *ui;

    QSharedPointer<Properties> m_properties;
    QVector3D m_pointOfInterest;
};

#endif // SEARCHRESULTWIDGET_H
