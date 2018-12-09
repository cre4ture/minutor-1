#include "searchentitywidget.h"
#include "ui_searchentitywidget.h"

#include "./chunkcache.h"
#include "./careeridentifier.h"

#include <QVariant>
#include <QTreeWidgetItem>

SearchEntityWidget::SearchEntityWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchEntityWidget)
  , m_input(input)
{
    ui->setupUi(this);

    connect(m_input.cache.get(), SIGNAL(chunkLoaded(bool,int,int)),
            this, SLOT(chunkLoaded(bool,int,int)));

    for (const auto& id: m_input.definitions.careerDefinitions->getKnownIds())
    {
        auto desc = m_input.definitions.careerDefinitions->getDescriptor(id);
        ui->cb_villager_type->addItem(desc.name);
    }
}

SearchEntityWidget::~SearchEntityWidget()
{
    delete ui;
}

enum
{
    CHUNK_SIZE = 16
};

void SearchEntityWidget::on_pb_search_clicked()
{
    ui->resultList->clearResults();
    ui->resultList->setPointOfInterest(m_input.posOfInterestProvider());

    const int radius = 1 + (ui->sb_radius->value() / CHUNK_SIZE);
    ui->progressBar->reset();
    ui->progressBar->setMaximum(radius * radius * 4);

    for (int z= -radius; z <= radius; z++)
    {
        for (int x = -radius; x <= radius; x++)
        {
             trySearchChunk(x, z);
        }
    }
}

void SearchEntityWidget::chunkLoaded(bool success, int x, int z)
{
    if (success)
    {
        QSharedPointer<Chunk> chunk = nullptr;
        if (m_input.cache->isLoaded(x, z, chunk))
        {
            searchChunk(*chunk);
        }
        else
        {
            ui->progressBar->setValue(ui->progressBar->value() + 1);
        }
    }
    else
    {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
    }
}

void SearchEntityWidget::trySearchChunk(int x, int z)
{
    QSharedPointer<Chunk> chunk = nullptr;
    if (m_input.cache->isLoaded(x, z, chunk))
    {
        searchChunk(*chunk);
        return;
    }

    chunk = m_input.cache->fetch(x, z);
    if (chunk != nullptr)
    {
        searchChunk(*chunk);
    }
}

void SearchEntityWidget::searchChunk(Chunk& chunk)
{
    const auto& map = chunk.getEntityMap();

    for(const auto& e: map)
    {
        EntityEvaluator evaluator(
                    EntityEvaluatorConfig(m_input.definitions,
                                          *ui->resultList,
                                          "",
                                          e,
                                          std::bind(&SearchEntityWidget::evaluateEntity, this, std::placeholders::_1)
                                          )
                    );
    }

    ui->progressBar->setValue(ui->progressBar->value() + 1);
}

bool SearchEntityWidget::evaluateEntity(EntityEvaluator &entity)
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

    return result;
}

bool SearchEntityWidget::findBuyOrSell(EntityEvaluator &entity, QString searchText, int index)
{
    bool foundBuy = false;
    auto offers = entity.getOffers();
    for (const auto& offer: offers)
    {
        auto splitOffer = offer.split("=>");
        foundBuy = (splitOffer.count() > index) && splitOffer[index].contains(searchText);
        if (foundBuy)
        {
            break;
        }
    }

    return foundBuy;
}



void SearchEntityWidget::on_resultList_jumpTo(const QVector3D &pos)
{
    emit jumpTo(pos);
}
