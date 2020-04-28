/** Copyright 2014 EtlamGit */
#ifndef ENTITY_H_
#define ENTITY_H_

#include <QSharedPointer>
#include "./overlayitem.h"

class Tag;
class PlayerInfo;

class Entity: public OverlayItem {
 public:
  explicit Entity(const Point& positionInfo);

  static QSharedPointer<OverlayItem> TryParse(const Tag* tag);

  virtual bool intersects(const OverlayItem::Cuboid& cuboid) const;
  virtual void draw(double offsetX, double offsetZ, double scale,
                    QPainter *canvas) const;
  virtual Point midpoint() const;
  void setExtraColor(const QColor& c) {extraColor = c;}

  static const int RADIUS = 5;

  explicit Entity(const PlayerInfo& player);
  explicit Entity(const QVector3D& plainInfo);

 protected:
  Entity() {}

 private:
  QColor extraColor;
  Point pos;
  QString display;
};

#endif  // ENTITY_H_
