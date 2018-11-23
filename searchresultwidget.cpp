#include "searchresultwidget.h"
#include "ui_searchresultwidget.h"

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
