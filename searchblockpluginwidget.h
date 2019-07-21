#ifndef SEARCHBLOCKPLUGINWIDGET_H
#define SEARCHBLOCKPLUGINWIDGET_H

#include "searchplugininterface.h"
#include "blockidentifier.h"

#include <QWidget>

namespace Ui {
class SearchBlockPluginWidget;
}

struct SearchBlockPluginWidgetConfigT
{
    SearchBlockPluginWidgetConfigT(BlockIdentifier& blockIdentifier_)
        : blockIdentifier(&blockIdentifier_)
        , parent(nullptr)
    {}

    BlockIdentifier* blockIdentifier;
    QWidget *parent;
};

class SearchBlockPluginWidget : public QWidget, public SearchPluginI
{
    Q_OBJECT

public:
    explicit SearchBlockPluginWidget(const SearchBlockPluginWidgetConfigT& config);
    ~SearchBlockPluginWidget();

    QWidget &getWidget() override;
    void searchChunk(SearchResultWidget &resultList, Chunk &chunk) override;

private:
    Ui::SearchBlockPluginWidget *ui;
    SearchBlockPluginWidgetConfigT m_config;
};

#endif // SEARCHBLOCKPLUGINWIDGET_H
