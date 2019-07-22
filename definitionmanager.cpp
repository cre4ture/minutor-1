/** Copyright (c) 2013, Sean Kasun */

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QStandardPaths>
#include <QtWidgets/QFileDialog>
#include <algorithm>
#include "./definitionmanager.h"
#include "./biomeidentifier.h"
#include "./blockidentifier.h"
#include "./dimensionidentifier.h"
#include "./entityidentifier.h"
#include "./flatteningconverter.h"
#include "./genericidentifier.h"
#include "./mapview.h"
#include "./json.h"
#include "./zipreader.h"
#include "./definitionupdater.h"

#include <boost/bimap.hpp>

template <typename L, typename R>
boost::bimap<L, R> makeBimap(std::initializer_list<typename boost::bimap<L, R>::value_type> list)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

static const boost::bimap<QString, Definition::Type> s_defintionLookupBi = makeBimap<QString, Definition::Type>(
{
    {"block", Definition::Converter}, // after merge question: needed anymore?
    {"biome", Definition::Biome},
    {"dimension", Definition::Dimension},
    {"entity", Definition::Entity},
    {"enchantment", Definition::Enchantment},
    {"profession", Definition::Profession},
    {"career", Definition::Career},
    {"pack", Definition::Pack},
    {"converter", Definition::Converter},
    {"flatblock", Definition::Block},
});



DefinitionManager::DefinitionManager(QWidget *parent) :
    QWidget(parent),
    isUpdating(false),
    dimensionManager(DimensionIdentifier::Instance()),
    blockManager(BlockIdentifier::Instance()),
    biomeManager(BiomeIdentifier::Instance()),
    entityManager(EntityIdentifier::Instance()),
    flatteningConverter(FlatteningConverter::Instance())
{
  m_managers[Definition::Dimension] = &dimensionManager;
  m_managers[Definition::Block] = &blockManager;
  m_managers[Definition::Biome] = &biomeManager;
  m_managers[Definition::Entity] = &entityManager;
  m_managers[Definition::Converter] = &flatteningConverter;

  setWindowFlags(Qt::Window);
  setWindowTitle(tr("Definitions"));

  QVBoxLayout *layout = new QVBoxLayout;
  QStringList labels;
  labels << tr("Name") << tr("Version") << tr("Type");
  table = new QTableWidget(0, 3);
  table->setHorizontalHeaderLabels(labels);
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->horizontalHeader()->setHighlightSections(false);
  table->verticalHeader()->hide();
  table->setShowGrid(false);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  layout->addWidget(table, 1);

  QWidget *buttonBar = new QWidget;
  QHBoxLayout *buttons = new QHBoxLayout;
  QPushButton *add = new QPushButton(tr("Add Pack..."));
  connect(add, SIGNAL(clicked()),
          this, SLOT(addPack()));
  buttons->addWidget(add);
  QPushButton *remove = new QPushButton(tr("Remove Pack"));
  connect(remove, SIGNAL(clicked()),
          this, SLOT(removePack()));
  connect(this, SIGNAL(packSelected(bool)),
          remove, SLOT(setEnabled(bool)));
  buttons->addWidget(remove);
  QPushButton *save = new QPushButton(tr("Export Pack..."));
  connect(save, SIGNAL(clicked()),
          this, SLOT(exportPack()));
  connect(this, SIGNAL(packSelected(bool)),
          save, SLOT(setEnabled(bool)));
  buttons->addWidget(save);
  buttonBar->setLayout(buttons);
  layout->addWidget(buttonBar, 0);

  emit packSelected(false);
  setLayout(layout);

  enchantmentManager = QSharedPointer<GenericIdentifier>::create();
  m_managers[Definition::Enchantment] = enchantmentManager.get();

  professionManager = QSharedPointer<GenericIdentifier>::create();
  m_managers[Definition::Profession] = professionManager.get();

  careerManager = QSharedPointer<GenericIdentifier>::create();
  m_managers[Definition::Career] = careerManager.get();

  // check & repair definition files
  this->checkAndRepair();

  // we load the definitions in backwards order for priority
  QSettings settings;
  sorted = settings.value("packs").toList();
  for (int i = sorted.length() - 1; i >= 0; i--)
    loadDefinition(sorted[i].toString());

  // hook up table selection signal
  connect(table,
          SIGNAL(currentItemChanged(QTableWidgetItem*, QTableWidgetItem*)),
          this, SLOT(selectedPack(QTableWidgetItem*, QTableWidgetItem*)));
  // fill out table
  refresh();
}

DefinitionManager::~DefinitionManager() {}

void DefinitionManager::checkAndRepair()
{
  // create definition data folder on disk
  QString destdir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
  QDir dir;
  dir.mkpath(destdir);

  // get known definition packs from application default settings storage
  QSettings settings;
  QList<QVariant> known_packs = settings.value("packs").toList();

  // in Minutor up to 2.2.0 we used hash without seed, which is incompatible to Qt5.12
  // force clean old hashed files generated without an extra seed
  // this assumes, that these hashes will never occur otherwise
  const QStringList old_hashed_list { "1050220429", "1241760321", "1443276275", "1798448990", "2422344665" };
  for ( const auto& old_hashed_file : old_hashed_list  ) {
    QString old_path = destdir + "/" + old_hashed_file + ".json";
    QFile::remove(old_path);
    known_packs.removeOne(old_path);
  }

  // repair when definitions is on disk, but missing in settings
  QDirIterator on_disk(destdir, QDir::Files | QDir::Readable);
  while (on_disk.hasNext()) {
    on_disk.next();
    if (!known_packs.contains(on_disk.filePath())) {
      known_packs.append(on_disk.filePath());
    }
  }

  // repair when definition is in settings, but file is missing
  for (const auto& def: known_packs) {
    if (!QFile::exists(def.toString()))
      known_packs.removeOne(def.toString());
  }

  // copy over built-in definitions if necessary
  QDirIterator build_in(":/definitions", QDir::Files | QDir::Readable);
  while (build_in.hasNext()) {
    build_in.next();
    installJson(build_in.filePath(), false, false);
  }
  // all changed definitions are now in sorted -> copy over
  known_packs.append(sorted);

  // store repaired list of definitions
  settings.setValue("packs", known_packs);
}

QSharedPointer<GenericIdentifier> DefinitionManager::enchantmentIdentifier()
{
    return enchantmentManager;
}

QSharedPointer<GenericIdentifier> DefinitionManager::careerIdentifier()
{
    return careerManager;
}

void DefinitionManager::refresh() {
  table->clearContents();
  table->setRowCount(0);

  for (int i = 0; i < sorted.length(); i++) {
    Definition &def = definitions[sorted[i].toString()];
    int row = table->rowCount();
    table->insertRow(row);
    QTableWidgetItem *name = new QTableWidgetItem(def.name);
    name->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    name->setData(Qt::UserRole, def.path);
    table->setItem(row, 0, name);
    QTableWidgetItem *ver = new QTableWidgetItem(def.version);
    ver->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ver->setData(Qt::UserRole, def.path);
    table->setItem(row, 1, ver);
    QTableWidgetItem *type = new QTableWidgetItem(convertDefinitionTypeToString(def.type));
    type->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    type->setData(Qt::UserRole, def.path);
    table->setItem(row, 2, type);
  }
}

DefinitionManager::ManagerTypePair DefinitionManager::getManagerFromString(const QString typeString)
{
    ManagerTypePair result;

    auto it = s_defintionLookupBi.left.find(typeString);
    if (it == s_defintionLookupBi.left.end())
    {
        return result;
    }

    result.type = it->get_right();
    auto itm = m_managers.find(result.type);
    if (itm == m_managers.end())
    {
        return result;
    }

    result.manager = *itm;

    return result;
}

QString DefinitionManager::convertDefinitionTypeToString(const Definition::Type type) const
{
    auto it = s_defintionLookupBi.right.find(type);
    if (it == s_defintionLookupBi.right.end())
    {
        return "invalid";
    }

    return it->get_left();
}

void DefinitionManager::selectedPack(QTableWidgetItem *item,
                                     QTableWidgetItem *) {
  emit packSelected(item != NULL);
  if (item != NULL)
    selected = item->data(Qt::UserRole).toString();
  else
    selected = QString();
}

void DefinitionManager::toggledPack(bool onoff) {
  if (definitions.contains(selected)) {
    Definition &def = definitions[selected];
    def.enabled = onoff;
    setManagerEnabled(def, onoff);
  }
  emit packsChanged();
  refresh();
}

void DefinitionManager::setManagerEnabled(const Definition& def, bool onoff)
{
    if (def.type == Definition::Pack)
    {
        for (auto pair: def.packSubDefId)
        {
            auto manager = m_managers[pair.first];
            manager->setDefinitionsEnabled(pair.second, onoff);
        }
    }
    else
    {
        IdentifierI* manager = m_managers[def.type];
        manager->setDefinitionsEnabled(def.id, onoff);
    }
}

void DefinitionManager::addPack() {
  QString packName = QFileDialog::getOpenFileName(this, tr("Open Pack"),
    QString(), tr("Definitions (*.zip *.json)"));
  if (!packName.isEmpty()) {
    if (packName.endsWith(".json", Qt::CaseInsensitive))  // raw json
      installJson(packName);
    else
      installZip(packName);
    emit packsChanged();
    QSettings settings;
    settings.setValue("packs", sorted);
    refresh();
  }
}

void DefinitionManager::installJson(QString path, bool overwrite,
                                    bool install) {
  QString destdir = QStandardPaths::writableLocation(
      QStandardPaths::DataLocation);

  JSONData *def;
  QFile f(path);
  f.open(QIODevice::ReadOnly);
  try {
    def = JSON::parse(f.readAll());
    f.close();
  } catch (JSONParseException e) {
    f.close();
    QMessageBox::warning(this, tr("Couldn't install %1").arg(path),
                         e.reason,
                         QMessageBox::Cancel);
    return;
  }

  QString key = def->at("name")->asString() + def->at("type")->asString();
  QString exeversion = def->at("version")->asString();
  delete def;
  QString dest = destdir + "/" + QString("%1").arg(qHash(key,42)) + ".json";

  // check if build in version is newer than version on disk
  if (QFile::exists(dest)) {
    QFile f(dest);
    f.open(QIODevice::ReadOnly);
    try {
      def = JSON::parse(f.readAll());
      f.close();
    } catch (JSONParseException e) {
      f.close();
      return;
    }
    QString fileversion = def->at("version")->asString();
    delete def;
    if (exeversion.compare(fileversion, Qt::CaseInsensitive) > 0) {
      // force overwriting outdated local copy
      QFile::remove(dest);
      QFile::copy(path, dest);
      QFile::setPermissions(dest, QFile::ReadOwner|QFile::WriteOwner);
    }
  }

  // import new definition (if file is not present)
  if (!QFile::exists(dest) || overwrite) {
    if (QFile::exists(dest) && install) {
      removeDefinition(dest);
      QFile::remove(dest);
    }
    if (!QFile::copy(path, dest)) {
      QMessageBox::warning(this, tr("Couldn't install %1").arg(path),
                           tr("Copy error"),
                           QMessageBox::Cancel);
      return;
    }
    // fix permissions since we might be copying a readonly resource.
    QFile::setPermissions(dest, QFile::ReadOwner|QFile::WriteOwner);
    sorted.prepend(dest);
    if (install)
      loadDefinition(dest);
  }
}

void DefinitionManager::installZip(QString path, bool overwrite,
                                   bool install) {
  QString destdir = QStandardPaths::writableLocation(
      QStandardPaths::DataLocation);
  ZipReader zip(path);
  if (!zip.open()) {
    QMessageBox::warning(this, tr("Couldn't install %1").arg(path),
                         tr("Corrupt zip"),
                         QMessageBox::Cancel);
    return;
  }
  // fetch the pack info
  JSONData *info;
  try {
    info = JSON::parse(zip.get("pack_info.json"));
  } catch (JSONParseException e) {
    QMessageBox::warning(this, tr("Couldn't install %1").arg(path),
                         tr("pack_info.json : %1").arg(e.reason),
                         QMessageBox::Cancel);
    zip.close();
    return;
  }
  // let's verify all the jsons in the pack
  for (int i = 0; i < info->at("data")->length(); i++) {
    JSONData *def;
    try {
      def = JSON::parse(zip.get(info->at("data")->at(i)->asString()));
      delete def;
    } catch (JSONParseException e) {
      QMessageBox::warning(this, tr("Couldn't install %1").arg(path),
                           tr("%1: %2")
                           .arg(info->at("data")->at(i)->asString(),
                                e.reason), QMessageBox::Cancel);
      delete info;
      zip.close();
      return;
    }
  }

  QString key = info->at("name")->asString() + info->at("type")->asString();
  delete info;
  QString dest = destdir + "/" + QString("%1").arg(qHash(key,42)) + ".zip";
  if (!QFile::exists(dest) || overwrite) {
    if (QFile::exists(dest) && install)
      removeDefinition(dest);
    if (!QFile::copy(path, dest)) {
      QMessageBox::warning(this,
                           tr("Couldn't install %1").arg(path),
                           tr("Copy error"),
                           QMessageBox::Cancel);
      return;
    }
    sorted.prepend(dest);
    if (install)
      loadDefinition(dest);
  }
}

void DefinitionManager::removePack() {
  // find selected pack
  if (definitions.contains(selected)) {
    int ret = QMessageBox::question(this, tr("Delete Pack"),
                                    tr("Are you sure you want to delete %1?")
                                    .arg(definitions[selected].name),
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No);
    if (ret == QMessageBox::Yes)
      removeDefinition(selected);
  }
}

void DefinitionManager::exportPack() {
  // find selected pack
  if (definitions.contains(selected)) {
    QString fname = definitions[selected].name;
    switch (definitions[selected].type) {
      case Definition::Block: fname+="_blocks"; break;
      case Definition::Biome: fname+="_biomes"; break;
      case Definition::Dimension: fname+="_dims"; break;
      default: break;
    }
    if (selected.endsWith(".zip"))
      fname += ".zip";
    else
      fname += ".json";
    QString dest = QFileDialog::getSaveFileName(this, tr("Save Pack As"),
      fname, tr("Definitions (*.zip *.json)"));
    if (!dest.isEmpty()) {
      if (!QFile::copy(selected, dest)) {
        QMessageBox::warning(this,
                             tr("Couldn't write to %1").arg(dest),
                             tr("Copy error"),
                             QMessageBox::Cancel);
      }
    }
  }
}

QSize DefinitionManager::minimumSizeHint() const {
  return QSize(300, 300);
}
QSize DefinitionManager::sizeHint() const {
  return QSize(400, 300);
}

void DefinitionManager::loadDefinition(QString path) {
  // determine if we're loading a single json or a pack
  if (path.endsWith(".json", Qt::CaseInsensitive)) {
    loadDefinition_json(path);
  } else {
    loadDefinition_zipped(path);
  }
}

void DefinitionManager::loadDefinition_json(QString path)
{
    JSONData *def;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    try {
      def = JSON::parse(f.readAll());
      f.close();
    } catch (JSONParseException e) {
      f.close();
      return;
    }


    Definition d;
    d.name = def->at("name")->asString();
    d.version = def->at("version")->asString();
    d.path = path;
    d.update = def->at("update")->asString();
    QString type = def->at("type")->asString();
    QString key = d.name + type;
    d.enabled = true;  // should look this up

    auto manager = getManagerFromString(type);
    if (!manager.manager)
    {
        return; //failed!
    }

    d.id = manager.manager->addDefinitions(
            dynamic_cast<JSONArray*>(def->at("data")));
    d.type = manager.type;

    definitions.insert(path, d);
    delete def;
}

void DefinitionManager::loadDefinition_zipped(QString path)
{
    ZipReader zip(path);
    if (!zip.open())
      return;
    JSONData *info;
    try {
      info = JSON::parse(zip.get("pack_info.json"));
    } catch (JSONParseException e) {
      zip.close();
      return;
    }
    Definition d;
    d.name = info->at("name")->asString();
    d.version = info->at("version")->asString();
    d.update = info->at("update")->asString();
    d.path = path;
    d.enabled = true;
    d.id = 0;
    d.type = Definition::Pack;
    QString key = d.name+"pack";
    for (int i = 0; i < info->at("data")->length(); i++) {
      JSONData *def;
      try {
        def = JSON::parse(zip.get(info->at("data")->at(i)->asString()));
      } catch (JSONParseException e) {
        continue;
      }
      QString type = def->at("type")->asString();

      if (type != "block")
      {
          auto manager = getManagerFromString(type);
          if (manager.manager)
          {
              int packId = -1;
              auto it = d.packSubDefId.find(manager.type);
              if (it != d.packSubDefId.end())
              {
                  packId = it->second;
              }

              d.packSubDefId[manager.type] = manager.manager->addDefinitions(
                          dynamic_cast<JSONArray*>(def->at("data")), packId);
          }
      }

      delete def;
    }
    definitions.insert(path, d);
    delete info;
    zip.close();
}

void DefinitionManager::removeDefinition(QString path) {
  // find the definition and remove it from disk
  Definition &def = definitions[path];
  if (def.path == path) {
    setManagerEnabled(def, false);
    definitions.remove(path);
    QFile::remove(path);
    sorted.removeOne(path);
    QSettings settings;
    settings.setValue("packs", sorted);
    emit packsChanged();
    refresh();
  }
}

void DefinitionManager::checkForUpdates() {
  // show update dialog
  if (!isUpdating)
    autoUpdate();
  if (!isUpdating) {  // nothing needs updating
    // hide update dialog
    // show completion
  }
}

void DefinitionManager::autoUpdate() {
  for (int i = 0; i < sorted.length(); i++) {
    QString name = sorted[i].toString();
    Definition &def = definitions[name];
    if (!def.update.isEmpty()) {
      isUpdating = true;
      auto updater = new DefinitionUpdater(name, def.update, def.version);
      connect(updater, SIGNAL(updated   (DefinitionUpdater *, QString, QString)),
              this,    SLOT  (updatePack(DefinitionUpdater *, QString, QString)));
      updateQueue.append(updater);
      updater->update();
    }
  }
  QSettings settings;
  settings.setValue("packupdate", QDateTime::currentDateTime());  // store current time
  settings.remove("packupdates");  // remove now unused entries from registry
}

void DefinitionManager::updatePack(DefinitionUpdater *updater,
                                   QString filename,
                                   QString version) {
  updateQueue.removeOne(updater);
  delete updater;

  if (updateQueue.isEmpty())
    emit updateFinished();
}
