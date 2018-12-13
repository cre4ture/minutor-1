#include "chunk.h"
#include "searchblockpluginwidget.h"
#include "ui_searchblockpluginwidget.h"

SearchBlockPluginWidget::SearchBlockPluginWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SearchBlockPluginWidget)
{
    ui->setupUi(this);
}

SearchBlockPluginWidget::~SearchBlockPluginWidget()
{
    delete ui;
}

QWidget &SearchBlockPluginWidget::getWidget()
{
    return *this;
}

void SearchBlockPluginWidget::searchChunk(SearchResultWidget &resultList, Chunk &chunk)
{

}
