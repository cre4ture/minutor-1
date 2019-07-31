#include "searchentitypluginwidget.h"
#include "ui_searchentitypluginwidget.h"

#include "./careeridentifier.h"
#include "chunk.h"

SearchEntityPluginWidget::SearchEntityPluginWidget(const SearchEntityPluginWidgetConfigT& config)
    : QWidget(config.parent)
    , ui(new Ui::SearchEntityPluginWidget)
    , m_config(config)
{
    ui->setupUi(this);

    for (const auto& id: m_config.definitions.careerDefinitions->getKnownIds())
    {
        auto desc = m_config.definitions.careerDefinitions->getDescriptor(id);
        ui->cb_villager_type->addItem(desc.name);
    }
}

SearchEntityPluginWidget::~SearchEntityPluginWidget()
{
    delete ui;
}

QWidget &SearchEntityPluginWidget::getWidget()
{
    return *this;
}

void SearchEntityPluginWidget::searchChunk(SearchResultWidget& resultList, Chunk &chunk)
{
    const auto& map = chunk.getEntityMap();

    for(const auto& e: map)
    {
        EntityEvaluator evaluator(
                    EntityEvaluatorConfig(m_config.definitions,
                                          resultList,
                                          "",
                                          e,
                                          std::bind(&SearchEntityPluginWidget::evaluateEntity, this, std::placeholders::_1)
                                          )
                    );
    }
}

bool SearchEntityPluginWidget::evaluateEntity(EntityEvaluator &entity)
{
    bool result = true;

    if (ui->check_villager_type->isChecked())
    {
        QString searchFor = ui->cb_villager_type->currentText();
        QString career = entity.getCareerName();
        result = result && (career.compare(searchFor, Qt::CaseInsensitive) == 0);
    }

    if (ui->check_buys->isChecked())
    {
        result = result && findBuyOrSell(entity, ui->cb_buys->currentText(), 0);
    }

    if (ui->check_sells->isChecked())
    {
        result = result && findBuyOrSell(entity, ui->cb_sells->currentText(), 1);
    }

    if (ui->check_entity_type->isChecked())
    {
        QString searchFor = ui->cb_entity_type->currentText();
        QString id = entity.getTypeId();
        result = result && (id.contains(searchFor, Qt::CaseInsensitive));
    }

    if (ui->check_special_param->isChecked())
    {
        result = result && findSpecialParam(entity, ui->cb_special_param->currentText());
    }

    return result;
}

bool SearchEntityPluginWidget::findBuyOrSell(EntityEvaluator &entity, QString searchText, int index)
{
    bool foundBuy = false;
    auto offers = entity.getOffers();
    for (const auto& offer: offers)
    {
        auto splitOffer = offer.split("=>");
        foundBuy = (splitOffer.count() > index) && splitOffer[index].contains(searchText, Qt::CaseInsensitive);
        if (foundBuy)
        {
            break;
        }
    }

    return foundBuy;
}

bool SearchEntityPluginWidget::findSpecialParam(EntityEvaluator &entity, QString searchText)
{
    QString special_params = entity.getSpecialParams();
    return special_params.contains(searchText, Qt::CaseInsensitive);
}

