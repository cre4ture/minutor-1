#ifndef SEARCHTEXTWIDGET_H
#define SEARCHTEXTWIDGET_H

#include <QWidget>

namespace Ui {
class SearchTextWidget;
}

class SearchTextWidget : public QWidget
{
  Q_OBJECT

public:
  explicit SearchTextWidget(QWidget *parent = nullptr);
  ~SearchTextWidget();

private:
  Ui::SearchTextWidget *ui;
};

#endif // SEARCHTEXTWIDGET_H
