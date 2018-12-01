#include "searchentitywidget.h"
#include "ui_searchentitywidget.h"

#include "./chunkcache.h"
#include "./careeridentifier.h"

#include <QVariant>
#include <QTreeWidgetItem>

static const QString prefixToBeRemoved = "minecraft:";

SearchEntityWidget::SearchEntityWidget(QSharedPointer<ChunkCache> cache,
                                       EntityDefitionsConfig definitions,
                                       QWidget *parent)
  : QWidget(parent)
  , ui(new Ui::SearchEntityWidget)
  , m_cache(cache)
  , m_definitions(definitions)
{
    ui->setupUi(this);

    connect(m_cache.get(), SIGNAL(chunkLoaded(bool,int,int)),
            this, SLOT(chunkLoaded(bool,int,int)));
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
        if (m_cache->isLoaded(x, z, chunk))
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
    if (m_cache->isLoaded(x, z, chunk))
    {
        searchChunk(*chunk);
        return;
    }

    chunk = m_cache->fetch(x, z);
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
                    EntityEvaluatorConfig(m_definitions,
                                          *ui->resultList,
                                          ui->le_searchText->text(),
                                          e)
                    );
    }

    ui->progressBar->setValue(ui->progressBar->value() + 1);
}


EntityEvaluator::EntityEvaluator(const EntityEvaluatorConfig& config)
    : m_config(config)
    , m_rootNode(QSharedPointer<QTreeWidgetItem>::create())
{
    m_creator.CreateTree(m_rootNode.get(), m_config.entity->properties());

    searchProperties();
}

QList<QString> EntityEvaluator::getOffers() const
{
    QList<QString> result;

    auto* node = getNodeFromPath("Offers/Recipes", *m_rootNode);
    if (!node)
    {
        return result;
    }

    for (int i = 0; i < node->childCount(); i++)
    {
        auto& currentReceipNode = *node->child(i);
        result.append(describeReceipe(currentReceipNode));
    }

    return result;
}

void EntityEvaluator::searchProperties()
{
    searchTreeNode("", *m_rootNode);
}

void EntityEvaluator::searchTreeNode(const QString prefix, const QTreeWidgetItem &node)
{
    auto keyText = prefix + node.text(0);
    auto valueText = node.text(1);
    QString offers = getOffers().join("|");
   // bool found = keyText.contains("sell.id") && valueText.contains(m_config.searchText); // enchanted_book
    //bool found = offers.contains(m_config.searchText);
    //bool found = isVillager() && getCareerName() == "Cleric";
    //bool found = getTypeId().contains("chicken");
    bool found = isVillager() && getCareerName() == "Farmer";
    if (found)
    {
        SearchResultItem result;
        result.properties = m_config.entity->properties();
        result.name = m_creator.GetSummary("[0]", m_config.entity->properties());
        result.pos.setX(m_config.entity->midpoint().x);
        result.pos.setY(m_config.entity->midpoint().y);
        result.pos.setZ(m_config.entity->midpoint().z);
        result.sells = offers;
        m_config.resultSink.addResult(result);
    }
    else
    {
        for (int i = 0; i < node.childCount(); i++)
        {
            searchTreeNode(keyText + ".", *node.child(i));
        }
    }
}

QString EntityEvaluator::describeReceipe(const QTreeWidgetItem &currentReceipNode) const
{
    QString result = "";

    auto* buyNode = getNodeFromPath("buy", currentReceipNode);
    if (buyNode)
    {
        result += describeReceipeItem(*buyNode);
    }

    auto* buyBNode = getNodeFromPath("buyB", currentReceipNode);
    if (buyBNode)
    {
        result += "," + describeReceipeItem(*buyBNode);
    }

    auto* sellNode = getNodeFromPath("sell", currentReceipNode);
    if (sellNode)
    {
        result += " => " + describeReceipeItem(*sellNode);
    }

    return result;
}

QString EntityEvaluator::describeReceipeItem(const QTreeWidgetItem &itemNode) const
{
    QString value = "";

    auto* itemIdNode = getNodeFromPath("id", itemNode);
    if (itemIdNode)
    {
        auto* itemCountNode = getNodeFromPath("Count", itemNode);
        int count = itemCountNode->text(1).toInt();
        if (count > 1)
        {
            value += QString::number(count) + "*";
        }

        QString id = itemIdNode->text(1);
        if (id.startsWith(prefixToBeRemoved))
        {
            id.remove(0, prefixToBeRemoved.size());
        }

        value += id;

        if (id == "enchanted_book")
        {
            value += "{";
            auto* enchantmentNode = getNodeFromPath("tag/StoredEnchantments", itemNode);
            if (enchantmentNode)
            {
                auto* enchantmendIdNode = getNodeFromPath("id", *enchantmentNode);
                if (enchantmendIdNode)
                {
                    int id = enchantmendIdNode->text(1).toInt();
                    QString name = m_config.definitions.enchantmentDefintions->getDescriptor(id).name;
                    value += name + " (" + enchantmendIdNode->text(1) + ")";
                }
                auto* enchantmendLevelNode = getNodeFromPath("lvl", *enchantmentNode);
                if (enchantmendLevelNode)
                {
                    value += "-lvl:" + enchantmendLevelNode->text(1);
                }
            }
            value += "}";
        }
    }

    return value;
}

const QTreeWidgetItem *EntityEvaluator::getNodeFromPath(const QString path, const QTreeWidgetItem &searchRoot)
{
    QStringList elements = path.split("/");

    return getNodeFromPath(elements.begin(), elements.end(), searchRoot);
}

const QTreeWidgetItem *EntityEvaluator::getNodeFromPath(QStringList::iterator iter, QStringList::iterator end, const QTreeWidgetItem &searchRoot)
{
    QString nextName = *iter;
    iter++;

    for (int i = 0; i < searchRoot.childCount(); i++)
    {
        auto *child = searchRoot.child(i);
        if (child && (child->text(0) == nextName))
        {
            if (iter != end)
            {
                return getNodeFromPath(iter, end, *child);
            }
            else
            {
                return child;
            }
        }
    }

    return nullptr;
}

const QString EntityEvaluator::getNodeValueFromPath(const QString path, const QTreeWidgetItem &searchRoot, QString defaultValue)
{
    auto* node = getNodeFromPath(path, searchRoot);
    if (node)
    {
        return node->text(1);
    }

    return defaultValue;
}

QString EntityEvaluator::getTypeId() const
{
    auto* itemIdNode = getNodeFromPath("id", *m_rootNode);
    if (itemIdNode)
    {
        return itemIdNode->text(1);
    }

    return "-";
}

bool EntityEvaluator::isVillager() const
{
    auto id = getTypeId();
    return (id == "minecraft:villager");
}

QString EntityEvaluator::getCareerName() const
{
    QString profession = getNodeValueFromPath("Profession", *m_rootNode, "-1");
    QString career = getNodeValueFromPath("Career", *m_rootNode, "-1");

    int iProfession = profession.toInt();
    int iCareer = career.toInt();

    if (iProfession >= 0 && iCareer >= 0)
    {
        int id = createCareerId(iProfession, iCareer);
        auto desc = m_config.definitions.careerDefinitions->getDescriptor(id);
        return desc.name;
    }

    return "-";
}

void SearchEntityWidget::on_resultList_jumpTo(const QVector3D &pos)
{
    emit jumpTo(pos);
}
