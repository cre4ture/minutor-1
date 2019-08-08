#include "chunk.h"
#include "searchblockpluginwidget.h"
#include "searchresultwidget.h"
#include "ui_searchblockpluginwidget.h"

#include <set>
#include <algorithm>

SearchBlockPluginWidget::SearchBlockPluginWidget(const SearchBlockPluginWidgetConfigT& config)
    : QWidget(config.parent)
    , ui(new Ui::SearchBlockPluginWidget)
    , m_config(config)
{
    ui->setupUi(this);

    auto idList = m_config.blockIdentifier->getKnownIds();

    QStringList nameList;
    nameList.reserve(idList.size());

    for (auto id: idList)
    {
        auto blockInfo = m_config.blockIdentifier->getBlockInfo(id);
        nameList.push_back(blockInfo.getName());
    }

    nameList.sort(Qt::CaseInsensitive);

    for (auto name: nameList)
    {
        ui->cb_Name->addItem(name);
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

bool SearchBlockPluginWidget::initSearch()
{
    const bool checkId = ui->check_blockId->isChecked();
    const bool checkName = ui->check_Name->isChecked();

    m_searchForIds.clear();

    if (checkId)
    {
        m_searchForIds.insert(ui->sb_id->value());
    }

    if (checkName)
    {
        const QString searchForName = ui->cb_Name->currentText();
        auto idList = m_config.blockIdentifier->getKnownIds();
        for (auto id: idList)
        {
            auto blockInfo = m_config.blockIdentifier->getBlockInfo(id);
            if (blockInfo.getName().contains(searchForName, Qt::CaseInsensitive))
            {
                m_searchForIds.insert(id);
            }
        }
    }

    return m_searchForIds.size() > 0;
}

SearchPluginI::ResultListT SearchBlockPluginWidget::searchChunk(Chunk &chunk)
{
    SearchPluginI::ResultListT results;

    if (m_searchForIds.size() == 0)
    {
        return results;
    }

    for (int z = 0; z < 16; z++)
    {
        for (int y = 0; y < 256; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                const Block bi = chunk.getBlockData(x,y,z);
                const auto it = m_searchForIds.find(bi.id);
                if (it != m_searchForIds.end())
                {
                    auto info = m_config.blockIdentifier->getBlockInfo(bi.id);

                    SearchResultItem item;
                    item.name = info.getName() + " (" + QString::number(bi.id) + ")";
                    item.pos = QVector3D(chunk.getChunkX() * 16 + x, y, chunk.getChunkZ() * 16 + z);
                    item.entity = QSharedPointer<Entity>::create(item.pos);
                    results.push_back(item);
                }
            }
        }
    }

    return results;
}
