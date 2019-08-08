/** Copyright (c) 2013, Sean Kasun */
#ifndef MINUTOR_H_
#define MINUTOR_H_

#include "playerinfos.h"

#include <QtWidgets/QMainWindow>
#include <QDir>
#include <QVariant>
#include <QSharedPointer>
#include <QSet>
#include <QVector3D>
#include <QTimer>

class QAction;
class QActionGroup;
class QMenu;
class QProgressDialog;
class MapView;
class LabelledSlider;
class DefinitionManager;
class DimensionIdentifier;
class Settings;
class DimensionInfo;
class WorldSave;
class Properties;
class OverlayItem;
class JumpTo;
class SearchEntityWidget;
class ChunkCache;
class SearchPluginI;
class AsyncTaskProcessorBase;

class Location {
 public:
  Location(double x, double z) : x(x), z(z) {}
  Location(const QVector3D& pos3D) : x(pos3D.x()), z(pos3D.z()) {}
  double x, z;

  Location operator/(int divider) const
  {
      return Location(x / divider, z / divider);
  }
};

class Minutor : public QMainWindow {
  Q_OBJECT

 public:
  Minutor();

  void loadWorld(QDir path);

  void savePNG(QString filename, bool autoclose = false,
               bool regionChecker = false, bool chunkChecker = false,
               int w_top = 0, int w_left = 0, int w_bottom = 0, int w_right = 0);

  void jumpToXZ(int blockX, int blockZ);  // jumps to the block coords
  void setViewLighting(bool value);       // set View->Ligthing
  void setViewMobspawning(bool value);    // set View->Mob_Spawning
  void setViewCavemode(bool value);       // set View->Cave_Mode
  void setViewDepthshading(bool value);   // set View->Depth_Shading
  void setViewBiomeColors(bool value);    // set View->Biome_Colors
  void setDepth(int value);               // set Depth-Slider

  MapView *getMapview() const;

private slots:
  void openWorld();
  void open();
  void closeWorld();
  void reload();
  void save();

  void jumpToPlayerLocation();
  void jumpToPlayersBedLocation();
  void jumpToSpawn();
  void followPlayer();

  void viewDimension(const DimensionInfo &dim);
  void toggleFlags();

  void about();

  void updateDimensions();
  void rescanWorlds();
  void saveProgress(QString status, double value);
  void saveFinished();
  void addOverlayItem(QSharedPointer<OverlayItem> item);
  void addOverlayItemType(QString type, QColor color, QString dimension = "");
  void showProperties(QVariant props);

  SearchEntityWidget* prepareSearchForm(const QSharedPointer<SearchPluginI> &searchPlugin);
  void searchBlock();
  void searchEntity();
  void triggerJumpToPosition(QVector3D pos);
  void highlightEntity(QSharedPointer<OverlayItem> item);

  void periodicUpdate();

signals:
  void worldLoaded(bool isLoaded);

 private:
  void updateChunksAroundPlayer(const Location& pos, size_t areaRadius);

  void createActions();
  void createMenus();
  void createStatusBar();
  void loadStructures(const QDir &dataPath);
  void populateEntityOverlayMenu();
  QKeySequence generateUniqueKeyboardShortcut(QString *actionName);

  QString getWorldName(QDir path);
  void getWorldList();

  QSharedPointer<AsyncTaskProcessorBase> threadpool;
  QSharedPointer<ChunkCache> cache;
  MapView *mapview;
  LabelledSlider *depth;
  QProgressDialog *progress;
  bool progressAutoclose;
  QVector<PlayerInfo> playerInfos;
  QVector3D spawnPoint;
  QSharedPointer<DimensionInfo> currentDimentionInfo;
  QString playerToFollow;

  QMenu *fileMenu, *worldMenu;
  QMenu *viewMenu, *jumpPlayerMenu, *dimMenu;
  QMenu *followPlayerMenu;
  QMenu *helpMenu;
  QMenu *structureOverlayMenu, *entityOverlayMenu;
  QMenu *searchMenu;

  QList<QAction *>worlds;
  QAction *openAct, *reloadAct, *saveAct, *exitAct;
  QAction *jumpSpawnAct;
  QList<QAction *>players;
  QAction *lightingAct, *mobSpawnAct, *caveModeAct, *depthShadingAct, *biomeColorsAct;
  QAction *manageDefsAct;
  QAction *refreshAct;
  QAction *aboutAct;
  QAction *settingsAct;
  QAction *updatesAct;
  QAction *jumpToAct;
  QList<QAction*> structureActions;
  QList<QAction*> entityActions;
  QAction *searchEntityAction;
  QAction *searchBlockAction;

  // loaded world data
  DefinitionManager *dm;
  Settings *settings;
  JumpTo *jumpTo;
  QDir currentWorld;

  QSet<QString> overlayItemTypes;

  QTimer periodicUpdateTimer;
};

#endif  // MINUTOR_H_
