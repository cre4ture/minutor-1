#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <QVector3D>

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

#endif // LOCATION_HPP
