#ifndef ENTITYEVALUATOR_H
#define ENTITYEVALUATOR_H

#include "propertietreecreator.h"

#include <QSharedPointer>

#include <functional>

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;
class OverlayItem;

class QTreeWidgetItem;

struct EntityDefitionsConfig
{
    EntityDefitionsConfig(const QSharedPointer<GenericIdentifier>& enchantmentDefintions_,
                          const QSharedPointer<GenericIdentifier>& careerDefinitions_)
        : enchantmentDefintions(enchantmentDefintions_)
        , careerDefinitions(careerDefinitions_)
    {}

    QSharedPointer<GenericIdentifier> enchantmentDefintions;
    QSharedPointer<GenericIdentifier> careerDefinitions;
};

class EntityEvaluator;

struct EntityEvaluatorConfig
{
    typedef std::function<bool(EntityEvaluator&)> SearchFunctionT;

    EntityEvaluatorConfig(const EntityDefitionsConfig& definitions_,
                          SearchResultWidget& resultSink_,
                          const QString& searchText_,
                          QSharedPointer<OverlayItem> entity_,
                          SearchFunctionT evalFunction_)
        : definitions(definitions_)
        , resultSink(resultSink_)
        , searchText(searchText_)
        , entity(entity_)
        , evalFunction(evalFunction_)
    {}

    EntityDefitionsConfig definitions;
    SearchResultWidget& resultSink;
    QString searchText;
    QSharedPointer<OverlayItem> entity;
    std::function<bool(EntityEvaluator&)> evalFunction;
};

class EntityEvaluator
{
public:
    EntityEvaluator(const EntityEvaluatorConfig& config);

    QList<QString> getOffers() const;
    QString getSpecialParams() const;

    static const QTreeWidgetItem *getNodeFromPath(const QString path, const QTreeWidgetItem &searchRoot);
    static const QTreeWidgetItem *getNodeFromPath(QStringList::iterator start, QStringList::iterator end, const QTreeWidgetItem &searchRoot);

    static const QString getNodeValueFromPath(const QString path, const QTreeWidgetItem &searchRoot, QString defaultValue);

    QString getTypeId() const;
    bool isVillager() const;
    QString getCareerName() const;

    QString getNamedAttribute(const QString& name) const;

private:
    EntityEvaluatorConfig m_config;
    QSharedPointer<QTreeWidgetItem> m_rootNode;
    PropertieTreeCreator m_creator;

    void searchProperties();
    void searchTreeNode(const QString prefix, const QTreeWidgetItem& node);

    QString describeReceipe(const QTreeWidgetItem& node) const;
    QString describeReceipeItem(const QTreeWidgetItem& node) const;

    void addResult();
};


#endif // ENTITYEVALUATOR_H
