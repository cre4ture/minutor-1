#ifndef MAPVIEWRENDERER_H
#define MAPVIEWRENDERER_H

#include "chunk.h"
#include "coordinateid.h"
#include "enumbitset.hpp"
#include "mapcamera.hpp"

#include <QImage>

enum class RenderStateT
{
  Empty,
  LoadingRequested,
  RenderingRequested,
};

class ChunkGroupCamC;
class ChunkGroupRendererC;

struct RenderGroupData
{
  RenderGroupData();

  void clear();

  RenderGroupData& init();

  RenderParams renderedFor;

  QImage renderedImg;
  QImage depthImg;
  QHash<ChunkID, QSharedPointer<Chunk::EntityMap> > entities;

  struct ChunkState
  {
    RenderParams renderedFor;
    Bitset<RenderStateT, uint8_t> flags;
  };

  QHash<ChunkID, ChunkState> states;
};

MapCamera CreateCameraForChunkGroup(const ChunkGroupID& cgid);

class ChunkGroupCamC
{
public:
  const ChunkGroupID cgid;
  const RenderGroupData& data;
  const MapCamera cam;

  ChunkGroupCamC(const RenderGroupData& data_, const ChunkGroupID cgid_)
    : cgid(cgid_)
    , data(data_)
    , cam(CreateCameraForChunkGroup(cgid))
  {
  }

  int getHeightAt(const TopViewPosition& midpoint)
  {
    const QPointF depthImgPixelF = cam.getPixelFromBlockCoordinates(TopViewPosition(midpoint.x, midpoint.z));
    const QPoint depthImgPixel(static_cast<int>(depthImgPixelF.x()), static_cast<int>(depthImgPixelF.y()));

    const int highY = data.depthImg.pixel(depthImgPixel) & 0xff;
    return highY;
  }
};

const QImage &getPlaceholder();
const QImage &getChunkGroupPlaceholder();

#endif // MAPVIEWRENDERER_H
