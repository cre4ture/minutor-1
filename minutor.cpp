/** Copyright (c) 2013, Sean Kasun */
#include "./minutor.h"
#include "./mapview.h"
#include "./labelledslider.h"
#include "./nbt.h"
#include "./json.h"
#include "./definitionmanager.h"
#include "./entityidentifier.h"
#include "./settings.h"
#include "./dimensionidentifier.h"
#include "./worldsave.h"
#include "./properties.h"
#include "./generatedstructure.h"
#include "./village.h"
#include "./jumpto.h"
#include "./pngexport.h"
#include "./searchchunkswidget.h"
#include "./playerinfos.h"
#include "searchentitypluginwidget.h"
#include "searchblockpluginwidget.h"
#include "prioritythreadpool.h"
#include "searchresultwidget.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QAction>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTreeWidget>
#include <QProgressDialog>
#include <QDir>
#include <QRegExp>
#include <QVector3D>

Minutor::Minutor()
    : threadpool(QSharedPointer<PriorityThreadPool>::create())
    , cache(QSharedPointer<ChunkCache>::create(threadpool))
    , searchMenu(nullptr)
    , searchEntityAction(nullptr)
    , searchBlockAction(nullptr)
    , listStructuresActionsMenu(nullptr)
    , periodicUpdateTimer()
{
  mapview = new MapView(threadpool, cache);
  mapview->attach(cache);
  connect(mapview, SIGNAL(hoverTextChanged(QString)),
          statusBar(), SLOT(showMessage(QString)));
  connect(mapview, SIGNAL(showProperties(QVariant)),
          this, SLOT(showProperties(QVariant)));
  connect(mapview, SIGNAL(addOverlayItemType(QString, QColor)),
          this, SLOT(addOverlayItemType(QString, QColor)));
  dm = new DefinitionManager(this);
  mapview->attach(dm);
  connect(dm,   SIGNAL(packsChanged()),
          this, SLOT(updateDimensions()));
  DimensionIdentifier *dimensions = &DimensionIdentifier::Instance();
  connect(dimensions, SIGNAL(dimensionChanged(const DimensionInfo &)),
          this, SLOT(viewDimension(const DimensionInfo &)));
  settings = new Settings(this);
  connect(settings, SIGNAL(settingsUpdated()),
          this, SLOT(rescanWorlds()));
  jumpTo = new JumpTo(this);

  if (settings->autoUpdate) {
    // get time of last update
    QSettings settings;
    QDateTime lastUpdateTime = settings.value("packupdate").toDateTime();

    // auto-update only once a week
    if (lastUpdateTime.addDays(7) < QDateTime::currentDateTime())
      dm->autoUpdate();
  }

  createActions();
  createMenus();
  createStatusBar();

  QBoxLayout *mainLayout;
  if (settings->verticalDepth) {
    mainLayout = new QHBoxLayout;
    depth = new LabelledSlider(Qt::Vertical);
    mainLayout->addWidget(mapview, 1);
    mainLayout->addWidget(depth);
  } else {
    mainLayout = new QVBoxLayout;
    depth = new LabelledSlider(Qt::Horizontal);
    mainLayout->addWidget(depth);
    mainLayout->addWidget(mapview, 1);
  }
  depth->setValue(255);
  mainLayout->setSpacing(0);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  connect(depth, SIGNAL(valueChanged(int)),
          mapview, SLOT(setDepth(int)));
  connect(mapview, SIGNAL(demandDepthChange(int)),
          depth, SLOT(changeValue(int)));
  connect(mapview, SIGNAL(demandDepthValue(int)),
          depth, SLOT(setValue(int)));
  connect(this, SIGNAL(worldLoaded(bool)),
          mapview, SLOT(setEnabled(bool)));
  connect(this, SIGNAL(worldLoaded(bool)),
          depth, SLOT(setEnabled(bool)));

  QWidget *central = new QWidget;
  central->setLayout(mainLayout);

  setCentralWidget(central);
  layout()->setContentsMargins(0, 0, 0, 0);

  setWindowTitle(qApp->applicationName());

  emit worldLoaded(false);

  periodicUpdateTimer.setInterval(1000);
  connect(&periodicUpdateTimer, SIGNAL(timeout()), this, SLOT(periodicUpdate()));
  periodicUpdateTimer.start();
}

Minutor::~Minutor() {
  // wait for sheduled tasks
  QThreadPool::globalInstance()->waitForDone();
}


void Minutor::openWorld() {
  QAction *action = qobject_cast<QAction*>(sender());
  if (action)
    loadWorld(action->data().toString());
}

void Minutor::open() {
  QString dirName = QFileDialog::getExistingDirectory(this, tr("Open World"));
  if (!dirName.isEmpty()) {
    QDir path(dirName);
    if (!path.exists("level.dat")) {
      QMessageBox::warning(this,
                           tr("Couldn't open world"),
                           tr("%1 is not a valid Minecraft world")
                           .arg(dirName),
                           QMessageBox::Cancel);
      return;
    }
    loadWorld(dirName);
  }
}

void Minutor::reload() {
  auto loc = mapview->getLocation();

  loadWorld(currentWorld);
  mapview->setLocation(loc.x, loc.y, loc.z, false, true);
}

void Minutor::save() {
  int w_top, w_left, w_right, w_bottom;
  WorldSave::findBounds(mapview->getWorldPath(),
                        &w_top, &w_left, &w_bottom, &w_right);
  PngExport pngoptions;
  pngoptions.setBounds(w_top, w_left, w_bottom, w_right);
  pngoptions.exec();
  if (pngoptions.result() == QDialog::Rejected)
    return;

  // get filname to save to
  QFileDialog fileDialog(this);
  fileDialog.setDefaultSuffix("png");
  QString filename = fileDialog.getSaveFileName(this, tr("Save world as PNG"),
                                                QString(), "PNG Images (*.png)");

  // check if filename was given
  if (filename.isEmpty())
    return;

  // add .png suffix if not present
  QFile file(filename);
  QFileInfo fileinfo(file);
  if (fileinfo.suffix().isEmpty()) {
    filename.append(".png");
  }

  // save world to PNG image
  savePNG(filename, false,
          pngoptions.getRegionChecker(), pngoptions.getChunkChecker(),
          pngoptions.getTop(), pngoptions.getLeft(),
          pngoptions.getBottom(), pngoptions.getRight());
}

void Minutor::savePNG(QString filename, bool autoclose,
                      bool regionChecker, bool chunkChecker,
                      int w_top, int w_left, int w_bottom, int w_right) {
  progressAutoclose = autoclose;
  if (!filename.isEmpty()) {
    WorldSave *ws = new WorldSave(filename, mapview,
                                  regionChecker, chunkChecker,
                                  w_top, w_left, w_bottom, w_right);
    progress = new QProgressDialog();
    progress->setCancelButton(nullptr);
    progress->setMaximum(100);
    progress->show();
    connect(ws, SIGNAL(progress(QString, double)),
            this, SLOT(saveProgress(QString, double)));
    connect(ws, SIGNAL(finished()),
            this, SLOT(saveFinished()));
    QThreadPool::globalInstance()->start(ws);
  }
}


void Minutor::saveProgress(QString status, double value) {
  progress->setValue(value*100);
  progress->setLabelText(status);
}

void Minutor::saveFinished() {
  progress->hide();
  delete progress;
  if (progressAutoclose)
    this->close();
}

void Minutor::closeWorld() {
  for (int i = 0; i < players.size(); i++) {
    jumpPlayerMenu->removeAction(players[i]);
    delete players[i];
  }
  players.clear();
  jumpPlayerMenu->setEnabled(false);
  followPlayerMenu->setEnabled(false);
  // clear dimensions menu
  DimensionIdentifier::Instance().removeDimensions(dimMenu);
  // clear overlays
  mapview->clearOverlayItems();
  // clear other stuff
  currentWorld = QString();
  emit worldLoaded(false);
}

void Minutor::jumpToPlayerLocation() {
  QAction *action = qobject_cast<QAction*>(sender());
  if (action) {
    QVector3D loc = playerInfos[action->data().toInt()].currentPosition;
    mapview->setLocation(loc.x(), loc.z());
  }
}

void Minutor::jumpToPlayersBedLocation()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
      QVector3D loc = playerInfos[action->data().toInt()].bedPosition;
      mapview->setLocation(loc.x(), loc.z());
    }
}

void Minutor::jumpToSpawn()
{
    mapview->setLocation(spawnPoint.x(), spawnPoint.z());
}

void Minutor::followPlayer()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action)
    {
        if (action->isChecked())
        {
            for (auto& a: followPlayerMenu->actions()) // uncheck all other players
            {
                if (a != action)
                {
                    a->setChecked(false);
                }
            }

            const auto& pinfo = playerInfos[action->data().toInt()];
            playerToFollow = pinfo.name;
        }
        else
        {
            playerToFollow = "";
        }
    }
}

void Minutor::jumpToXZ(int blockX, int blockZ) {
  mapview->setLocation(blockX, blockZ);
}

void Minutor::setViewLighting(bool value) {
  lightingAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewMobspawning(bool value) {
  mobSpawnAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewCavemode(bool value) {
  caveModeAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewDepthshading(bool value) {
  depthShadingAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewBiomeColors(bool value) {
  biomeColorsAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewSeaGroundMode(bool value) {
  seaGroundAct->setChecked(value);
  toggleFlags();
}

void Minutor::setViewSingleLayer(bool value) {
  singleLayerAct->setChecked(value);
  toggleFlags();
}

void Minutor::setDepth(int value) {
  depth->setValue(value);
}

void Minutor::toggleFlags() {
  int flags = 0;

  if (lightingAct->isChecked())     flags |= MapView::flgLighting;
  if (mobSpawnAct->isChecked())     flags |= MapView::flgMobSpawn;
  if (caveModeAct->isChecked())     flags |= MapView::flgCaveMode;
  if (depthShadingAct->isChecked()) flags |= MapView::flgDepthShading;
  if (biomeColorsAct->isChecked())  flags |= MapView::flgBiomeColors;
  if (seaGroundAct->isChecked())    flags |= MapView::flgSeaGround;
  if (singleLayerAct->isChecked())  flags |= MapView::flgSingleLayer;
  if (seaGroundAct->isChecked())    flags |= MapView::flgSeaGround;
  mapview->setFlags(flags);

  QSet<QString> overlayTypes;
  for (auto action : structureActions) {
    if (action->isChecked()) {
      overlayTypes.insert(action->data().toMap()["type"].toString());
    }
  }
  for (auto action : entityActions) {
    if (action->isChecked()) {
      overlayTypes.insert(action->data().toString());
    }
  }
  mapview->setVisibleOverlayItemTypes(overlayTypes);
}

void Minutor::listStructures()
{
    QAction* sendingAction = dynamic_cast<QAction*>(sender());
    if (sendingAction)
    {
        QString type = sendingAction->data().toMap()["type"].toString();
        auto list = mapview->getOverlayItems(type);

        SearchResultWidget* window = new SearchResultWidget();
        window->setPointOfInterest(mapview->getLocation().getPos3D());

        for (const auto& item: list)
        {
            if (item)
            {
                SearchResultItem lineitem;
                lineitem.pos = item->midpoint().toVector3D();
                lineitem.entity = item;
                lineitem.properties = item->properties();
                lineitem.name = item->dimension() + "." + item->display();
                window->addResult(lineitem);
            }
        }

        connect(window, SIGNAL(jumpTo(QVector3D)),
                this, SLOT(triggerJumpToPosition(QVector3D))
                );

        connect(window, SIGNAL(highlightEntities(QVector<QSharedPointer<OverlayItem> >)),
                this, SLOT(highlightEntities(QVector<QSharedPointer<OverlayItem> >))
                );

        window->setWindowTitle(type);
        window->showNormal();
    }
}

void Minutor::viewDimension(const DimensionInfo &dim) {

    currentDimentionInfo = QSharedPointer<DimensionInfo>::create(dim);

  for (auto action : structureActions) {
    QString dimension = action->data().toMap()["dimension"].toString();
    if (dimension.isEmpty() ||
        !dimension.compare(dim.name, Qt::CaseInsensitive)) {
      action->setVisible(true);
    } else {
      action->setVisible(false);
    }
  }
  mapview->setDimension(dim.path, dim.scale);
}

void Minutor::about() {
  QMessageBox::about(this, tr("About %1").arg(qApp->applicationName()),
                     tr("<b>%1</b> v%2<br/>\n"
                        "&copy; Copyright %3, %4")
                     .arg(qApp->applicationName())
                     .arg(qApp->applicationVersion())
                     .arg(2019)
                     .arg(qApp->organizationName()));
}

void Minutor::updateDimensions() {
  DimensionIdentifier::Instance().getDimensions(currentWorld, dimMenu, this);
}

void Minutor::createActions() {
  getWorldList();

  // [File]
  openAct = new QAction(tr("&Open..."), this);
  openAct->setShortcut(tr("Ctrl+O"));
  openAct->setStatusTip(tr("Open a world"));
  connect(openAct, SIGNAL(triggered()),
          this,    SLOT(open()));

  reloadAct = new QAction(tr("&Reload"), this);
  reloadAct->setShortcut(tr("F5"));
  reloadAct->setStatusTip(tr("Reload current world"));
  connect(reloadAct, SIGNAL(triggered()),
          this,      SLOT(reload()));
  connect(this,      SIGNAL(worldLoaded(bool)),
          reloadAct, SLOT(setEnabled(bool)));

  saveAct = new QAction(tr("&Save PNG..."), this);
  saveAct->setShortcut(tr("Ctrl+S"));
  saveAct->setStatusTip(tr("Save as PNG"));
  connect(saveAct, SIGNAL(triggered()),
          this,    SLOT(save()));
  connect(this,    SIGNAL(worldLoaded(bool)),
          saveAct, SLOT(setEnabled(bool)));

  exitAct = new QAction(tr("E&xit"), this);
  exitAct->setShortcut(tr("Ctrl+Q"));
  exitAct->setStatusTip(tr("Exit %1").arg(qApp->applicationName()));
  connect(exitAct, SIGNAL(triggered()),
          this,    SLOT(close()));

  // [View->Jump]
  jumpSpawnAct = new QAction(tr("Jump to &Spawn"), this);
  jumpSpawnAct->setShortcut(tr("F1"));
  jumpSpawnAct->setStatusTip(tr("Jump to world spawn"));
  connect(jumpSpawnAct, SIGNAL(triggered()),
          this,         SLOT(jumpToSpawn()));
  connect(this,         SIGNAL(worldLoaded(bool)),
          jumpSpawnAct, SLOT(setEnabled(bool)));

  jumpToAct = new QAction(tr("&Jump To"), this);
  jumpToAct->setShortcut(tr("F2"));
  jumpToAct->setStatusTip(tr("Jump to a location"));
  connect(jumpToAct, SIGNAL(triggered()),
          jumpTo,    SLOT(show()));
  connect(this,      SIGNAL(worldLoaded(bool)),
          jumpToAct, SLOT(setEnabled(bool)));


  // [View->Modes]
  lightingAct = new QAction(tr("&Lighting"), this);
  lightingAct->setCheckable(true);
  lightingAct->setShortcut(tr("Ctrl+L"));
  lightingAct->setStatusTip(tr("Toggle lighting on/off"));
  connect(lightingAct, SIGNAL(triggered()),
          this,        SLOT(toggleFlags()));

  mobSpawnAct = new QAction(tr("&Mob spawning"), this);
  mobSpawnAct->setCheckable(true);
  mobSpawnAct->setShortcut(tr("Ctrl+M"));
  mobSpawnAct->setStatusTip(tr("Toggle show mob spawning on/off"));
  connect(mobSpawnAct, SIGNAL(triggered()),
          this,        SLOT(toggleFlags()));

  caveModeAct = new QAction(tr("&Cave Mode"), this);
  caveModeAct->setCheckable(true);
  caveModeAct->setShortcut(tr("Ctrl+C"));
  caveModeAct->setStatusTip(tr("Toggle cave mode on/off"));
  connect(caveModeAct, SIGNAL(triggered()),
          this,        SLOT(toggleFlags()));

  depthShadingAct = new QAction(tr("&Depth shading"), this);
  depthShadingAct->setCheckable(true);
  depthShadingAct->setShortcut(tr("Ctrl+D"));
  depthShadingAct->setStatusTip(tr("Toggle shading based on relative depth"));
  connect(depthShadingAct, SIGNAL(triggered()),
          this,            SLOT(toggleFlags()));

  biomeColorsAct = new QAction(tr("&Biome Colors"), this);
  biomeColorsAct->setCheckable(true);
  biomeColorsAct->setShortcut(tr("Ctrl+B"));
  biomeColorsAct->setStatusTip(tr("Toggle draw biome colors or block colors"));
  connect(biomeColorsAct, SIGNAL(triggered()),
          this,           SLOT(toggleFlags()));
  
  seaGroundAct = new QAction(tr("Sea &Ground Mode"), this);
  seaGroundAct->setCheckable(true);
  seaGroundAct->setShortcut(tr("Ctrl+G"));
  seaGroundAct->setStatusTip(tr("Toggle sea ground mode on/off"));
  connect(seaGroundAct, SIGNAL(triggered()),
          this,           SLOT(toggleFlags()));

  singleLayerAct = new QAction(tr("Single Layer"), this);
  singleLayerAct->setCheckable(true);
  //singleLayerAct->setShortcut(tr("Ctrl+L"));  // both S and L are already used
  singleLayerAct->setStatusTip(tr("Toggle single layer on/off"));
  connect(singleLayerAct, SIGNAL(triggered()),
          this,           SLOT(toggleFlags()));

  // [View->Others]
  refreshAct = new QAction(tr("Refresh"), this);
  refreshAct->setShortcut(tr("F2"));
  refreshAct->setStatusTip(tr("Reloads all chunks, "
                              "but keeps the same position / dimension"));
  connect(refreshAct, SIGNAL(triggered()),
          mapview,    SLOT(clearCache()));

  manageDefsAct = new QAction(tr("Manage &Definitions..."), this);
  manageDefsAct->setStatusTip(tr("Manage block and biome definitions"));
  connect(manageDefsAct, SIGNAL(triggered()),
          dm,            SLOT(show()));

  // [Help]
  aboutAct = new QAction(tr("&About"), this);
  aboutAct->setStatusTip(tr("About %1").arg(qApp->applicationName()));
  connect(aboutAct, SIGNAL(triggered()),
          this,     SLOT(about()));

  settingsAct = new QAction(tr("Settings..."), this);
  settingsAct->setStatusTip(tr("Change %1 Settings")
                            .arg(qApp->applicationName()));
  connect(settingsAct, SIGNAL(triggered()),
          settings,    SLOT(show()));

  updatesAct = new QAction(tr("Check for updates..."), this);
  updatesAct->setStatusTip(tr("Check for updated packs"));
  connect(updatesAct, SIGNAL(triggered()),
          dm,         SLOT(checkForUpdates()));

  searchEntityAction = new QAction(tr("Search entity"), this);
  connect(searchEntityAction, SIGNAL(triggered()), this, SLOT(searchEntity()));

  searchBlockAction = new QAction(tr("Search block"), this);
  connect(searchBlockAction, SIGNAL(triggered()), this, SLOT(searchBlock()));

  listStructuresActionsMenu = new QMenu(tr("List structures"), this);
}

// actionName will be modified, a "&" is added
QKeySequence Minutor::generateUniqueKeyboardShortcut(QString *actionName) {
  // generate a unique keyboard shortcut
  QKeySequence sequence;
  // test all letters in given name
  QString testName(*actionName);
  for (int ampPos=0; ampPos < testName.length(); ++ampPos) {
    QChar c = testName[ampPos];
    sequence = QKeySequence(QString("Ctrl+")+c);
    for (auto m : menuBar()->findChildren<QMenu*>()) {
      for (auto a : m->actions()) {
        if (a->shortcut() == sequence) {
          sequence = QKeySequence();
          break;
        }
      }
      if (sequence.isEmpty())
        break;  // already eliminated this as a possbility
    }
    if (!sequence.isEmpty()) {  // not eliminated, this one is ok
      *actionName = testName.mid(0, ampPos) + "&" + testName.mid(ampPos);
      break;
    }
  }
  return sequence;
}

void Minutor::populateEntityOverlayMenu() {
  EntityIdentifier &ei = EntityIdentifier::Instance();
  for (auto it = ei.getCategoryList().constBegin();
       it != ei.getCategoryList().constEnd(); it++) {
    QString category = it->first;
    QColor catcolor = it->second;

    QString actionName = category;
    QKeySequence sequence = generateUniqueKeyboardShortcut(&actionName);

    QPixmap pixmap(16, 16);
    QColor solidColor(catcolor);
    solidColor.setAlpha(255);
    pixmap.fill(solidColor);

    entityActions.push_back(new QAction(pixmap, actionName, this));
    entityActions.last()->setShortcut(sequence);
    entityActions.last()->setStatusTip(tr("Toggle viewing of %1")
                                       .arg(category));
    entityActions.last()->setEnabled(true);
    entityActions.last()->setData("Entity."+category);
    entityActions.last()->setCheckable(true);
    entityOverlayMenu->addAction(entityActions.last());
    connect(entityActions.last(), SIGNAL(triggered()),
            this, SLOT(toggleFlags()));
  }
}


void Minutor::createMenus() {
  // [File]
  fileMenu = menuBar()->addMenu(tr("&File"));
  worldMenu = fileMenu->addMenu(tr("&Open World"));

  worldMenu->addActions(worlds);
  if (worlds.size() == 0)  // no worlds found
    worldMenu->setEnabled(false);

  fileMenu->addAction(openAct);
  fileMenu->addAction(reloadAct);
  fileMenu->addSeparator();
  fileMenu->addAction(saveAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exitAct);

  // [View]
  viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(jumpSpawnAct);
  viewMenu->addAction(jumpToAct);
  jumpPlayerMenu = viewMenu->addMenu(tr("&Jump to Player"));
  jumpPlayerMenu->setEnabled(false);
  followPlayerMenu = viewMenu->addMenu(tr("Follow player"));
  followPlayerMenu->setEnabled(false);
  dimMenu = viewMenu->addMenu(tr("&Dimension"));
  dimMenu->setEnabled(false);
  // [View->Modes]
  viewMenu->addSeparator();
  viewMenu->addAction(lightingAct);
  viewMenu->addAction(mobSpawnAct);
  viewMenu->addAction(caveModeAct);
  viewMenu->addAction(depthShadingAct);
  viewMenu->addAction(biomeColorsAct);
  viewMenu->addAction(seaGroundAct);
  viewMenu->addAction(singleLayerAct);
  // [View->Overlay]
  viewMenu->addSeparator();
  structureOverlayMenu = viewMenu->addMenu(tr("&Structure Overlay"));
  entityOverlayMenu    = viewMenu->addMenu(tr("&Entity Overlay"));
  populateEntityOverlayMenu();

  viewMenu->addSeparator();
  viewMenu->addAction(refreshAct);
  viewMenu->addAction(manageDefsAct);

  //menuBar()->addSeparator();

  // [Search]
  searchMenu = menuBar()->addMenu(tr("&Search"));
  searchMenu->addAction(searchEntityAction);
  searchMenu->addAction(searchBlockAction);
  searchMenu->addMenu(listStructuresActionsMenu);

  // [Help]
  helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(aboutAct);
  helpMenu->addSeparator();
  helpMenu->addAction(settingsAct);
  helpMenu->addAction(updatesAct);
}

void Minutor::createStatusBar() {
  statusBar()->showMessage(tr("Ready"));
}

QString Minutor::getWorldName(QDir path) {
  if (!path.exists("level.dat"))  // no level.dat?  no world
    return QString();

  NBT level(path.filePath("level.dat"));
  return level.at("Data")->at("LevelName")->toString();
}


void Minutor::getWorldList() {
  QDir mc(settings->mcpath);
  if (!mc.cd("saves"))
    return;

  QDirIterator it(mc);
  int key = 1;
  while (it.hasNext()) {
    it.next();
    if (it.fileInfo().isDir()) {
      QString name = getWorldName(it.filePath());
      if (!name.isNull()) {
        QAction *w = new QAction(this);
        w->setText(name);
        w->setData(it.filePath());
        if (key < 10) {
          w->setShortcut("Ctrl+"+QString::number(key));
          key++;
        }
        connect(w, SIGNAL(triggered()),
                this, SLOT(openWorld()));
        worlds.append(w);
      }
    }
  }
}

MapView *Minutor::getMapview() const
{
  return mapview;
}

void Minutor::loadWorld(QDir path) {
  // cleanup current state (just in case)
  closeWorld();
  currentWorld = path;

  NBT level(path.filePath("level.dat"));

  auto data = level.at("Data");
  // add level name to window title
  setWindowTitle(qApp->applicationName() + " - " +
                 data->at("LevelName")->toString());
  // save world spawn
  spawnPoint.setX(data->at("SpawnX")->toDouble());
  spawnPoint.setY(data->at("SpawnY")->toDouble());
  spawnPoint.setZ(data->at("SpawnZ")->toDouble());

  // show saved players
  auto playerInfoList = loadPlayerInfos(path);
  for (auto player: playerInfoList)
  {
      QAction *p = new QAction(this);
      p->setText(player.name);
      connect(p, SIGNAL(triggered()),
              this, SLOT(jumpToPlayerLocation()));
      players.append(p);
      if (player.hasBed) {  // player has a bed
        p = new QAction(this);
        p->setText(player.name+"'s Bed");
        connect(p, SIGNAL(triggered()),
                this, SLOT(jumpToPlayersBedLocation()));
        players.append(p);
      }
  }
  jumpPlayerMenu->addActions(players);
  jumpPlayerMenu->setEnabled(playerInfoList.size() > 0);

  for (auto player: playerInfoList)
  {
      QAction *p = new QAction(this);
      p->setText(player.name);
      p->setCheckable(true);
      connect(p, SIGNAL(triggered()),
              this, SLOT(followPlayer()));
      followPlayerMenu->addAction(p);
  }
  followPlayerMenu->setEnabled(playerInfoList.size() > 0);

  if (path.cd("data")) {
    loadStructures(path);
    path.cdUp();
  }

  // show dimensions
  DimensionIdentifier::Instance().getDimensions(path, dimMenu, this);
  emit worldLoaded(true);
  jumpToSpawn();
  toggleFlags();
}

void Minutor::rescanWorlds() {
  worlds.clear();
  getWorldList();
  worldMenu->clear();
  worldMenu->addActions(worlds);
  worldMenu->setEnabled(worlds.count() != 0);
  // we don't care about the auto-update toggle, since that only happens
  // on startup anyway.
}

void Minutor::addOverlayItemType(QString type, QColor color,
                                 QString dimension) {
  if (!overlayItemTypes.contains(type)) {
    overlayItemTypes.insert(type);
    QList<QString> path = type.split('.');
    QList<QString>::const_iterator pathIt, nextIt, endPathIt = path.end();
    nextIt = path.begin();
    nextIt++;  // skip first part
    pathIt = nextIt++;
    QMenu* cur = structureOverlayMenu;

    // generate a nested menu structure to match the path
    while (nextIt != endPathIt) {
      QList<QMenu*> results =
          cur->findChildren<QMenu*>(*pathIt, Qt::FindDirectChildrenOnly);
      if (results.empty()) {
        cur = cur->addMenu("&" + *pathIt);
        cur->setObjectName(*pathIt);
      } else {
        cur = results.front();
      }
      pathIt = ++nextIt;
    }

    // generate a unique keyboard shortcut
    QString actionName = path.last();
    QKeySequence sequence = generateUniqueKeyboardShortcut(&actionName);

    QPixmap pixmap(16, 16);
    QColor solidColor(color);
    solidColor.setAlpha(255);
    pixmap.fill(solidColor);

    QMap<QString, QVariant> entityData;
    entityData["type"] = type;
    entityData["dimension"] = dimension;

    auto listAction = new QAction(actionName, this);
    listAction->setData(entityData);
    connect(listAction, SIGNAL(triggered()), this, SLOT(listStructures()));

    listStructuresActionsMenu->addAction(listAction);

    structureActions.push_back(new QAction(pixmap, actionName, this));
    structureActions.last()->setShortcut(sequence);
    structureActions.last()->setStatusTip(tr("Toggle viewing of %1")
                                          .arg(type));
    structureActions.last()->setEnabled(true);
    structureActions.last()->setData(entityData);
    structureActions.last()->setCheckable(true);
    cur->addAction(structureActions.last());
    connect(structureActions.last(), SIGNAL(triggered()),
            this, SLOT(toggleFlags()));
  }
}

void Minutor::addOverlayItem(QSharedPointer<OverlayItem> item) {
  // create menu entries (if necessary)
  addOverlayItemType(item->type(), item->color(), item->dimension());

//  const OverlayItem::Point& p = item->midpoint();
//  overlayItems[item->type()].insertMulti(QPair<int, int>(p.x, p.z), item);

  mapview->addOverlayItem(item);
}

void Minutor::showProperties(QVariant props)
{
  if (!props.isNull()) {
    Properties * propView = new Properties(this);
    propView->DisplayProperties(props);
    connect(propView, SIGNAL(onBoundingBoxSelected(QVector3D,QVector3D)), this, SLOT(highlightBoundingBox(QVector3D,QVector3D)));
    propView->show();
  }
}

SearchChunksWidget* Minutor::prepareSearchForm(const QSharedPointer<SearchPluginI>& searchPlugin)
{
    SearchChunksWidget* form = new SearchChunksWidget(SearchEntityWidgetInputC(threadpool, cache,
                                              [this](){ return mapview->getLocation().getPos3D(); },
                                              searchPlugin
    ));

    auto villageList = mapview->getOverlayItems("Structure.Village");
    form->setAttribute(Qt::WA_DeleteOnClose);

    connect(form, SIGNAL(jumpTo(QVector3D)),
            this, SLOT(triggerJumpToPosition(QVector3D))
            );

    connect(form, SIGNAL(highlightEntities(QVector<QSharedPointer<OverlayItem> >)),
            this, SLOT(highlightEntities(QVector<QSharedPointer<OverlayItem> >))
            );

    return form;
}

void Minutor::searchBlock()
{
    auto searchPlugin = QSharedPointer<SearchBlockPluginWidget>::create(SearchBlockPluginWidgetConfigT(BlockIdentifier::Instance()));
    auto searchBlockForm = prepareSearchForm(searchPlugin);
    searchBlockForm->showNormal();
}

void Minutor::searchEntity()
{
    auto searchPlugin = QSharedPointer<SearchEntityPluginWidget>::create(SearchEntityPluginWidgetConfigT(EntityDefitionsConfig(
                                                                                                             dm->enchantmentIdentifier(),
                                                                                                             dm->careerIdentifier()
                                                                                                             )));
    auto searchEntityForm = prepareSearchForm(searchPlugin);
    searchEntityForm->showNormal();
}

void Minutor::triggerJumpToPosition(QVector3D pos)
{
    mapview->setLocation(pos.x(), pos.y(), pos.z(), true, false);
}

void Minutor::highlightEntities(QVector<QSharedPointer<OverlayItem> > items)
{
  mapview->updateSearchResultPositions(items);
}

void Minutor::highlightBoundingBox(QVector3D from, QVector3D to)
{
  auto structure = QSharedPointer<GeneratedStructure>::create();
  structure->setBounds(OverlayItem::Point(from), OverlayItem::Point(to));

  highlightEntities({structure});
}

void Minutor::periodicUpdate()
{
    playerInfos = loadPlayerInfos(currentWorld);
    for (auto& player: playerInfos)
    {
        if (currentDimentionInfo->id == player.dimention)
        {
            updateChunksAroundPlayer(Location(player.currentPosition), 32);
        }
        else
        {
            auto dimInfo = DimensionIdentifier::Instance().getDimentionInfo(player.dimention);

            double scaleChange = (double)dimInfo.scale / (double)currentDimentionInfo->scale;
            player.currentPosition *= scaleChange;
        }

        if ((playerToFollow != "") && (playerToFollow == player.name))
        {
            mapview->setLocation(player.currentPosition.x(), player.currentPosition.y(), player.currentPosition.z(), true, false);
        }
    }

    mapview->updatePlayerPositions(playerInfos);
}

enum {
    CHUNKSIZE = 16
};

void Minutor::updateChunksAroundPlayer(const Location &pos, size_t areaRadius)
{
    auto id = ChunkID::fromCoordinates(static_cast<int>(pos.x), static_cast<int>(pos.z));
    const int chunkRadius = 1 + static_cast<int>(areaRadius) / CHUNKSIZE;

    ChunkCache::Locker locker(*cache);

    for (int dx = -chunkRadius; dx < chunkRadius; dx++)
    {
        for (int dz = -chunkRadius; dz < chunkRadius; dz++)
        {
            QSharedPointer<Chunk> chunk;
            ChunkID chunkToSearchID(id.getX() + dx, id.getZ() + dz);
            locker.fetch(chunk, chunkToSearchID, ChunkCache::FetchBehaviour::FORCE_UPDATE);
        }
    }
}

void Minutor::loadStructures(const QDir &dataPath) {
  // attempt to parse all of the files in the data directory, looking for
  // generated structures
  for (auto &fileName : dataPath.entryList(QStringList() << "*.dat")) {
    NBT file(dataPath.filePath(fileName));
    auto data = file.at("data");

    auto items = GeneratedStructure::tryParseDatFile(data);
    for (auto &item : items) {
      addOverlayItem(item);
    }

    if (items.isEmpty()) {
      // try parsing it as a villages.dat file
      int underidx = fileName.lastIndexOf('_');
      int dotidx = fileName.lastIndexOf('.');
      QString dimension = "overworld";
      if (underidx > 0) {
        dimension = fileName.mid(underidx + 1, dotidx - underidx - 1);
      }
      items = Village::tryParseDatFile(data, dimension);

      for (auto &item : items) {
        addOverlayItem(item);
      }
    }
  }
}
