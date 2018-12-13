#ifndef SEARCHBLOCKPLUGINWIDGET_H
#define SEARCHBLOCKPLUGINWIDGET_H

#include "searchplugininterface.h"

#include <QWidget>

namespace Ui {
class SearchBlockPluginWidget;
}

class SearchBlockPluginWidget : public QWidget, public SearchPluginI
{
    Q_OBJECT

public:
    explicit SearchBlockPluginWidget(QWidget *parent = nullptr);
    ~SearchBlockPluginWidget();

    QWidget &getWidget() override;
    void searchChunk(SearchResultWidget &resultList, Chunk &chunk) override;

private:
    Ui::SearchBlockPluginWidget *ui;

};

#endif // SEARCHBLOCKPLUGINWIDGET_H
