/** Copyright (c) 2013, Sean Kasun */
#ifndef DEFINITIONMANAGER_H_
#define DEFINITIONMANAGER_H_

#include <QtWidgets/QWidget>
#include <QHash>
#include <QString>
#include <QList>
#include <QVariant>
#include <QDateTime>

class QTableWidget;
class QTableWidgetItem;
class QCheckBox;
class BiomeIdentifier;
class BlockIdentifier;
class DimensionIdentifier;
class EntityIdentifier;
class MapView;
class JSONData;
class DefinitionUpdater;
class GenericIdentifier;
class IdentifierI;

struct Definition {
  QString name;
  QString version;
  QString path;
  QString update;
  enum Type {
      First,
      Block = First,
      Biome,
      Dimension,
      Entity,
      Enchantment,
      Profession,
      Career,
      Last = Career,
      Pack
  };
  Type type;
  int id;
  bool enabled;
  // for packs only
  std::map<Type, int> packSubDefId;
};

class DefinitionManager : public QWidget {
  Q_OBJECT

 public:
  explicit DefinitionManager(QWidget *parent = 0);
  ~DefinitionManager();
  void attachMapView(MapView *mapview);
  QSize minimumSizeHint() const;
  QSize sizeHint() const;

  QSharedPointer<BlockIdentifier> blockIdentifier();
  BiomeIdentifier *biomeIdentifier();
  DimensionIdentifier *dimensionIdentifer();
  QSharedPointer<GenericIdentifier> enchantmentIdentifier();
  QSharedPointer<GenericIdentifier> careerIdentifier();
  void autoUpdate();

 signals:
  void packSelected(bool on);
  void packsChanged();
  void updateFinished();

 public slots:
  void selectedPack(QTableWidgetItem *, QTableWidgetItem *);
  void toggledPack(bool on);
  void addPack();
  void removePack();
  void exportPack();
  void checkForUpdates();
  void updatePack(DefinitionUpdater *updater,
                  QString filename,
                  QString version);

 private:
  QTableWidget *table;
  QList<QCheckBox *>checks;
  void installJson(QString path, bool overwrite = true, bool install = true);
  void installZip(QString path, bool overwrite = true, bool install = true);
  void loadDefinition(QString path);
  void loadDefinition_json(QString path);
  void loadDefinition_zipped(QString path);
  void loadDefinition(JSONData *, int pack = -1);
  void removeDefinition(QString path);
  void refresh();
  QHash<QString, Definition> definitions;
  BiomeIdentifier *biomeManager;  // todo: migrate to reference to singleton
  QSharedPointer<BlockIdentifier> blockManager;  // todo: migrate to reference to singleton
  DimensionIdentifier *dimensionManager;  // todo: migrate to reference to singleton
  EntityIdentifier &entityManager;
  QSharedPointer<GenericIdentifier> enchantmentManager;
  QSharedPointer<GenericIdentifier> professionManager;
  QSharedPointer<GenericIdentifier> careerManager;
  QString selected;
  QList<QVariant> sorted;

  QMap<Definition::Type, IdentifierI*> m_managers;

  struct ManagerTypePair
  {
      ManagerTypePair()
          : manager(nullptr)
          , type(Definition::Block)
      {}

      IdentifierI* manager;
      Definition::Type type;
  };

  ManagerTypePair getManagerFromString(const QString typeString);
  QString convertDefinitionTypeToString(const Definition::Type type) const;

  bool isUpdating;
  QList<DefinitionUpdater *> updateQueue;

  void setManagerEnabled(const Definition& def, bool onoff);
};

#endif  // DEFINITIONMANAGER_H_
