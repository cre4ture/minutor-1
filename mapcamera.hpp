#ifndef MAPCAMERA_HPP
#define MAPCAMERA_HPP

#include <QRectF>

template<typename _numberT>
struct TopViewPosition_t;

template<>
struct TopViewPosition_t<int>
{
  TopViewPosition_t()
    : x(0)
    , z(0)
  {}

  TopViewPosition_t(int _x, int _z)
      : x(_x)
      , z(_z)
  {}

  int x;
  int z;
};

template<>
struct TopViewPosition_t<double>
{
  TopViewPosition_t()
    : x(0)
    , z(0)
  {}

  TopViewPosition_t(double _x, double _z)
      : x(_x)
      , z(_z)
  {}

  double x;
  double z;

  TopViewPosition_t<int> floor()
  {
    return TopViewPosition_t<int>(::floor(x), ::floor(z));
  }
};

using TopViewPosition = TopViewPosition_t<double>;

struct MapCamera
{
  TopViewPosition centerpos_blocks;
  QSize size_pixels;
  double zoom;

  MapCamera() {}

  MapCamera(double bx, double bz, const QSize& size_pixels_, double zoom_)
    : centerpos_blocks(bx, bz)
    , size_pixels(size_pixels_)
    , zoom(zoom_)
  {}

  TopViewPosition transformPixelToBlockCoordinates(QPoint pixel_pos) const;

  QPointF getPixelFromBlockCoordinates(TopViewPosition block_pos) const;
};

#endif // MAPCAMERA_HPP
