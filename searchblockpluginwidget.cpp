#include "chunk.h"
#include "searchblockpluginwidget.h"
#include "searchresultwidget.h"
#include "ui_searchblockpluginwidget.h"

#include <set>

SearchBlockPluginWidget::SearchBlockPluginWidget(const SearchBlockPluginWidgetConfigT& config)
    : QWidget(config.parent)
    , ui(new Ui::SearchBlockPluginWidget)
    , m_config(config)
{
    ui->setupUi(this);

    auto idList = m_config.blockIdentifier->getKnownIds();
    for (auto id: idList)
    {
        auto blockInfo = m_config.blockIdentifier->getBlock(id, 0);
        ui->cb_Name->addItem(blockInfo.getName());
    }
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
    const bool checkId = ui->check_blockId->isChecked();
    const bool checkName = ui->check_Name->isChecked();

    std::set<quint32> searchForIds;

    if (checkId)
    {
        searchForIds.insert(ui->sb_id->value());
    }

    if (checkName)
    {
        const QString searchForName = ui->cb_Name->currentText();
        auto idList = m_config.blockIdentifier->getKnownIds();
        for (auto id: idList)
        {
            auto blockInfo = m_config.blockIdentifier->getBlock(id, 0);
            if (blockInfo.getName().contains(searchForName, Qt::CaseInsensitive))
            {
                searchForIds.insert(id);
            }
        }
    }

    if (searchForIds.size() == 0)
    {
        return;
    }

    for (int z = 0; z < 16; z++)
    {
        for (int y = 0; y < 256; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                const Block bi = chunk.getBlockData(x,y,z);
                const auto it = searchForIds.find(bi.id);
                if (it != searchForIds.end())
                {
                    auto info = m_config.blockIdentifier->getBlock(bi.id, bi.bd);

                    SearchResultItem item;
                    item.name = info.getName() + " (" + QString::number(bi.id) + ")";
                    item.pos = QVector3D(chunk.getChunkX() * 16 + x, y, chunk.getChunkZ() * 16 + z);
                    resultList.addResult(item);
                }
            }
        }
    }
}
