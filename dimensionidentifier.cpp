/** Copyright (c) 2013, Sean Kasun */

#include <QDirIterator>
#include <QtWidgets/QMenu>
#include "./dimensionidentifier.h"
#include "./json.h"

class DimensionDef {
 public:
  DimensionDef() {}
  QString name;
  QString path;
  int scale;
  int id;
  bool regex;
  bool enabled;
};


// --------- --------- --------- ---------
// DimensionIdentifier
// --------- --------- --------- ---------

DimensionIdentifier::DimensionIdentifier() {
  group = NULL;
}

DimensionIdentifier::~DimensionIdentifier() {
  for (int i = 0; i < packs.length(); i++) {
    for (int j = 0; j < packs[i].length(); j++)
      delete packs[i][j];
  }
}

DimensionIdentifier& DimensionIdentifier::Instance() {
  static DimensionIdentifier singleton;
  return singleton;
}

void DimensionIdentifier::setDefinitionsEnabled(int pack, bool enabled)
{
    if (pack < 0) return;
    int len = packs[pack].length();
    for (int i = 0; i < len; i++)
      packs[pack][i]->enabled = enabled;
}

int DimensionIdentifier::addDefinitions(JSONArray *defs, int pack) {
  if (pack == -1) {
    pack = packs.length();
    packs.append(QList<DimensionDef*>());
  }

  int len = defs->length();
  for (int i = 0; i < len; i++) {
    JSONObject *d = dynamic_cast<JSONObject *>(defs->at(i));
    DimensionDef *dim = new DimensionDef();
    dim->enabled = true;
    if (d->has("name"))
      dim->name = d->at("name")->asString();
    else
      dim->name = "Unknown";
    if (d->has("path"))
      dim->path = d->at("path")->asString();
    else
      dim->path = ".";
    if (d->has("scale"))
      dim->scale = d->at("scale")->asNumber();
    else
      dim->scale = 1;
    if (d->has("regex"))
      dim->regex = d->at("regex")->asBool();
    else
      dim->regex = false;
    dim->id = (int)d->getChildAsValueOrDefault_t<double>("id", 0);
    definitions.append(dim);
    packs[pack].append(dim);
  }
  return pack;
}

void DimensionIdentifier::removeDimensions(QMenu *menu) {
  for (int i = 0; i < items.count(); i++) {
    menu->removeAction(items[i]);
    delete items[i];
  }
  items.clear();
  dimensions.clear();
  foundDimensions.clear();
  menu->setEnabled(false);
  if (group != NULL) {
    delete group;
    group = NULL;
  }
}

DimensionInfo DimensionIdentifier::getDimentionInfo(int dimId)
{
    for (const auto& dim: dimensions)
    {
        if (dim.id == dimId)
        {
            return dim;
        }
    }

    return DimensionInfo("",1,"",0);
}

void DimensionIdentifier::getDimensions(QDir path, QMenu *menu,
                                        QObject *parent) {
  // first get the currently selected dimension so it doesn't change
  QString current;
  for (int i = 0; i < items.length(); i++)
    if (items[i]->isChecked())
      current = dimensions[i].path;
  removeDimensions(menu);
  group = new QActionGroup(parent);

  for (int i = 0; i < definitions.length(); i++) {
    auto& currentDefintion = definitions[i];
    if (currentDefintion->enabled) {
      // check path for regex
      if (currentDefintion->regex) {
        QDirIterator it(path.absolutePath(), QDir::Dirs);
        QRegExp rx(currentDefintion->path);
        while (it.hasNext()) {
          it.next();
          if (rx.indexIn(it.fileName()) != -1) {
            QString name = currentDefintion->name;
            for (int c = 0; c < rx.captureCount(); c++)
              name = name.arg(rx.cap(c + 1));
            addDimension(path, it.fileName(), name, currentDefintion->scale, currentDefintion->id,
                         parent);
          }
        }
      } else {
        addDimension(path, currentDefintion->path, currentDefintion->name,
                     currentDefintion->scale, currentDefintion->id, parent);
      }
    }
  }
  menu->addActions(items);
  if (items.count() > 0) {
    bool changed = true;
    // locate our old selected item
    for (int i = 0; i < items.length(); i++) {
      if (dimensions[items[i]->data().toInt()].path == current) {
        items[i]->setChecked(true);
        changed = false;
        break;
      }
    }
    if (changed) {
      items.first()->setChecked(true);
      emit dimensionChanged(dimensions[items.first()->data().toInt()]);
    }
    menu->setEnabled(true);
  }
}

void DimensionIdentifier::addDimension(QDir path, QString dir, QString name,
                                       int scale, int dimId, QObject *parent) {
  if (!path.exists(dir))
    return;

  if (foundDimensions.contains(dir))
    return;

  path.cd(dir);
  if (path.exists("region")) {  // is it a used dimension?
    QAction *d = new QAction(parent);
    d->setText(name);
    d->setData(dimensions.count());
    dimensions.append(DimensionInfo(path.absolutePath(), scale, name, dimId));
    d->setCheckable(true);
    parent->connect(d, SIGNAL(triggered()),
                    this, SLOT(viewDimension()));
    group->addAction(d);
    items.append(d);
    foundDimensions.insert(dir, true);
  }
  path.cdUp();
}

void DimensionIdentifier::viewDimension() {
  QAction *action = qobject_cast<QAction*>(sender());
  if (action)
    emit dimensionChanged(dimensions[action->data().toInt()]);
}
