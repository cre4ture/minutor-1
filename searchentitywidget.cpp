#include "searchentitywidget.h"
#include "ui_searchentitywidget.h"

SearchEntityWidget::SearchEntityWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SearchEntityWidget)
{
    ui->setupUi(this);
}

SearchEntityWidget::~SearchEntityWidget()
{
    delete ui;
}
