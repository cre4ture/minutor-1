#include "searchtextwidget.h"
#include "ui_searchtextwidget.h"

SearchTextWidget::SearchTextWidget(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::SearchTextWidget)
{
  ui->setupUi(this);
}

SearchTextWidget::~SearchTextWidget()
{
  delete ui;
}
