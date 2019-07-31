#ifndef SEARCHENTITYPLUGINWIDGET_H
#define SEARCHENTITYPLUGINWIDGET_H

#include "entityevaluator.h"
#include "searchplugininterface.h"

#include <QWidget>

namespace Ui {
class SearchEntityPluginWidget;
}

struct SearchEntityPluginWidgetConfigT
{
    SearchEntityPluginWidgetConfigT(const EntityDefitionsConfig& definitions_)
        : definitions(definitions_)
        , parent(nullptr)
    {}

    EntityDefitionsConfig definitions;
    QWidget *parent;
};

class SearchEntityPluginWidget : public QWidget, public SearchPluginI
{
    Q_OBJECT

public:
    explicit SearchEntityPluginWidget(const SearchEntityPluginWidgetConfigT& config);
    ~SearchEntityPluginWidget() override;

    QWidget &getWidget() override;

    void searchChunk(SearchResultWidget &resultList, Chunk &chunk) override;

private:
    Ui::SearchEntityPluginWidget *ui;

    SearchEntityPluginWidgetConfigT m_config;

    bool evaluateEntity(EntityEvaluator &entity);
    bool findBuyOrSell(EntityEvaluator& entity, QString searchText, int index);
    bool findSpecialParam(EntityEvaluator& entity, QString searchText);
};

#endif // SEARCHENTITYPLUGINWIDGET_H
