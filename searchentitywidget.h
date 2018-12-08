#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"

#include <QWidget>

namespace Ui {
class SearchEntityWidget;
}

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;

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

    static const QTreeWidgetItem *getNodeFromPath(const QString path, const QTreeWidgetItem &searchRoot);
    static const QTreeWidgetItem *getNodeFromPath(QStringList::iterator start, QStringList::iterator end, const QTreeWidgetItem &searchRoot);

    static const QString getNodeValueFromPath(const QString path, const QTreeWidgetItem &searchRoot, QString defaultValue);

    QString getTypeId() const;
    bool isVillager() const;
    QString getCareerName() const;

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

class SearchEntityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchEntityWidget(QSharedPointer<ChunkCache> cache,
                                EntityDefitionsConfig definitions,
                                QWidget *parent = nullptr);
    ~SearchEntityWidget();

signals:
    void jumpTo(QVector3D pos);

private slots:
    void on_pb_search_clicked();

    void chunkLoaded(bool success, int x, int z);

    void on_resultList_jumpTo(const QVector3D &);

private:
    Ui::SearchEntityWidget *ui;
    QSharedPointer<ChunkCache> m_cache;
    EntityDefitionsConfig m_definitions;

    void trySearchChunk(int x, int z);

    void searchChunk(Chunk &chunk);

    bool evaluateEntity(EntityEvaluator& entity);
    bool findBuyOrSell(EntityEvaluator& entity, QString searchText, int index);
};

#endif // SEARCHENTITYWIDGET_H
