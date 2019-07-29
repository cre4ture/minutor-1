/** Copyright (c) 2013, Sean Kasun */
#ifndef DIMENSIONIDENTIFIER_H_
#define DIMENSIONIDENTIFIER_H_

#include "identifierinterface.h"

#include <QString>
#include <QList>
#include <QHash>
#include <QDir>

class QMenu;
class QAction;
class QActionGroup;
class JSONArray;

class DimensionInfo {
 public:
  DimensionInfo(QString path, int scale, QString name, int id)
      : path(path)
      , scale(scale)
      , name(name)
      , id(id)
  {}
  QString path;
  int scale;
  QString name;
  int id;
};

class DimensionDef;

class DimensionIdentifier : public QObject, public IdentifierI {
  Q_OBJECT

 public:
  // singleton: access to global usable instance
  static DimensionIdentifier &Instance();

  int addDefinitions(JSONArray *, int pack = -1) override;
  void setDefinitionsEnabled(int pack, bool enabled) override;
  void getDimensions(QDir path, QMenu *menu, QObject *parent);
  void removeDimensions(QMenu *menu);

  DimensionInfo getDimentionInfo(int dimId);

 signals:
  void dimensionChanged(const DimensionInfo &dim);

 private slots:
  void viewDimension();

 private:
  // singleton: prevent access to constructor and copyconstructor
  DimensionIdentifier();
  ~DimensionIdentifier();
  DimensionIdentifier(const DimensionIdentifier &);
  DimensionIdentifier &operator=(const DimensionIdentifier &);

  void addDimension(QDir path, QString dir, QString name, int scale, int dimId,
                    QObject *parent);
  QList<QAction *> items;
  QList<DimensionInfo> dimensions;
  QList<DimensionDef*> definitions;
  QList<QList<DimensionDef*> > packs;
  QActionGroup *group;

  QHash<QString, bool> foundDimensions;
};

#endif  // DIMENSIONIDENTIFIER_H_
