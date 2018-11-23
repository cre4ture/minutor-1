#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include <QWidget>

namespace Ui {
class SearchEntityWidget;
}

class SearchEntityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchEntityWidget(QWidget *parent = nullptr);
    ~SearchEntityWidget();

private:
    Ui::SearchEntityWidget *ui;
};

#endif // SEARCHENTITYWIDGET_H
