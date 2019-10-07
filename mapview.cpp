/** Copyright (c) 2013, Sean Kasun */

#include "./mapview.h"
#include "./chunkcache.h"
#include "./chunkrenderer.h"
#include "./definitionmanager.h"
#include "./blockidentifier.h"
#include "./biomeidentifier.h"
#include "./clamp.h"
#include "./chunkrenderer.h"
#include "asynctaskprocessorbase.hpp"

#include <QObject>
#include <QRunnable>
#include <QPainter>
#include <QResizeEvent>
#include <QMessageBox>
#include <assert.h>

class DrawHelper
{
public:
  const double x;
  const double z;
  const QRect imageSize;
  const double zoom;

  const double chunksize;

  const int centerchunkx;
  const int centerchunkz;

  int startx; // first chunk left
  int startz; // first chunk top

  int blockswide; // width in chunks
  int blockstall; // height in chunks

  double x1; // first coordinate left
  double z1; // first coordinate top
  double x2;
  double z2;

  DrawHelper(const double x_, const double z_, const double zoom_, const QRect image)
    : x(x_)
    , z(z_)
    , imageSize(image)
    , zoom(zoom_)
    , chunksize(16* zoom)
    // first find the center block position
    , centerchunkx((int)floor(x / 16.0))
    , centerchunkz((int)floor(z / 16.0))
  {
    // and the center of the screen
    int centerx = image.width() / 2;
    int centery = image.height() / 2;
    // and align for panning
    centerx -= (x - centerchunkx * 16) * zoom;
    centery -= (z - centerchunkz * 16) * zoom;
    // now calculate the topleft block on the screen
    startx = centerchunkx - floor(centerx / chunksize) - 1;
    startz = centerchunkz - floor(centery / chunksize) - 1;
    // and the dimensions of the screen in blocks
    blockswide = image.width() / chunksize + 3;
    blockstall = image.height() / chunksize + 3;


    double halfviewwidth = image.width() / 2 / zoom;
    double halvviewheight = image.height() / 2 / zoom;
    x1 = x - halfviewwidth;
    z1 = z - halvviewheight;
    x2 = x + halfviewwidth;
    z2 = z + halvviewheight;
  }

  explicit DrawHelper(MapView& parent)
    : DrawHelper(parent.x, parent.z, parent.zoom, parent.imageChunks.rect())
  {

  }
};

class DrawHelper2
{
public:
  static const int chunkSizeOrig = 16;

  DrawHelper2(DrawHelper& h_, QImage& imageBuffer)
                : h(h_)
                , canvas(&imageBuffer)
                , centerchunkx(floor(h.x / chunkSizeOrig))
                , centerchunkz(floor(h.z / chunkSizeOrig))
  {
  }

  void drawChunk_Map(int x, int z, const QSharedPointer<RenderedChunk> &renderedChunk);

  QPainter& getCanvas() { return canvas; }

protected:
  DrawHelper& h;
  QPainter canvas;

  // first find the center chunk
  const int centerchunkx;
  const int centerchunkz;
};



class DrawHelper3: public DrawHelper2
{
public:
  static const int chunkSizeOrig = 16;

  DrawHelper3(DrawHelper& h_, MapView& parent)
                : DrawHelper2(h_, parent.imageChunks)
                , m_parent(parent)
                , canvas_entities(&parent.imageOverlays)
                , canvas_players(&parent.image_players)
    {
    }

    void drawChunkEntities(const RenderedChunk &chunk);

    void drawOverlayItemToPlayersCanvas(const QVector<QSharedPointer<OverlayItem> >& items)
    {
        for (const auto& item: items)
        {
          if (item != nullptr)
          {
            item->draw(h.x1, h.z1, h.zoom, &canvas_players);
          }
        }
    }

private:
    MapView& m_parent;
    QPainter canvas_entities;
    QPainter canvas_players;
};

class MapViewCache
{
public:
    MapViewCache(QSize blockSize)
        : m_blockSize(blockSize)
    {}

    class MyMath
    {
    public:
        int first_block;
        int block_count;
        int first_block_offset;
        int last_block_cutoff;

        MyMath(const size_t blockSize, int start, int length)
        {
            first_block = start / blockSize;
            block_count = 1 + length / blockSize;
            first_block_offset = start - (first_block * blockSize);
            last_block_cutoff = (first_block_offset + length) - (block_count * blockSize);
        }
    };

    void drawTo(QPainter& canvas, const QRect& destination, const QRect& source)
    {
        double scale_x = (double)destination.width() / (double)source.width();
        double scale_y = (double)destination.height() / (double)source.height();
        MyMath math_x(m_blockSize.width(), source.left(), source.width());
        MyMath math_y(m_blockSize.height(), source.top(), source.height());

        for (size_t y = 0; y < math_y.block_count; y++)
        {
            for (size_t x = 0; x < math_x.block_count; x++)
            {
                ChunkID id(math_x.first_block + x, math_y.first_block + y);
                auto it = m_blocks.find(id);
                if (it != m_blocks.end())
                {
                    QImage& image = it.value();

                    QRect source_image_rect(QPoint(0,0), m_blockSize);

                    QRectF targetRect = source_image_rect;
                    targetRect.moveTo(-math_x.first_block_offset, -math_y.first_block_offset);
                    targetRect.setLeft(targetRect.left() * scale_x);
                    targetRect.setRight(targetRect.right() * scale_x);
                    targetRect.setTop(targetRect.top() * scale_y);
                    targetRect.setBottom(targetRect.bottom() * scale_y);

                    canvas.drawImage(targetRect, image, source_image_rect);
                }
            }
        }
    }

private:
    QSize m_blockSize;
    QMap<ChunkID, QImage> m_blocks;
};

MapView::MapView(const QSharedPointer<AsyncTaskProcessorBase> &threadpool, QWidget *parent)
  : QWidget(parent)
  , cache(ChunkCache::Instance())
  , zoom(1.0)
  , updateTimer()
  , dragging(false)
  , m_asyncRendererPool(threadpool)
{
  havePendingToolTip = false;

  connect(&updateTimer, SIGNAL(timeout()),
          this, SLOT(regularUpdate()));

  updateTimer.setInterval(30);
  updateTimer.start();

  depth = 255;
  scale = 1;
  connect(cache.get(), SIGNAL(structureFound(QSharedPointer<GeneratedStructure>)),
          this,   SLOT  (addStructureFromChunk(QSharedPointer<GeneratedStructure>)));
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  getPlaceholder(); // force init

  // calculate exponential function for cave shade
  float cavesum = 0.0;
  for (int i=0; i<CAVE_DEPTH; i++) {
    caveshade[i] = 1/exp(i/(CAVE_DEPTH/2.0));
    cavesum += caveshade[i];
  }
  for (int i=0; i<CAVE_DEPTH; i++) {
    caveshade[i] = 1.5 * caveshade[i] / cavesum;
  }
}

QSize MapView::minimumSizeHint() const {
  return QSize(300, 300);
}
QSize MapView::sizeHint() const {
  return QSize(400, 400);
}

void MapView::attach(DefinitionManager *dm) {
  this->dm = dm;
}

void MapView::attach(QSharedPointer<ChunkCache> chunkCache_)
{
    cache = chunkCache_;

    connect(cache.get(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&, int, int)),
            this, SLOT(chunkUpdated(const QSharedPointer<Chunk>&, int, int)));
}

void MapView::setLocation(double x, double z) {
  setLocation(x, depth, z, false, true);
}

void MapView::setLocation(double x, int y, double z, bool ignoreScale, bool useHeight) {
  this->x = ignoreScale ? x : x / scale;
  this->z = ignoreScale ? z : z / scale;
  if (useHeight == true && depth != y) {
    emit demandDepthValue(y);
  } else {
  }
}

MapView::BlockLocation MapView::getLocation()
{
  currentLocation.x = x;
  currentLocation.y = depth;
  currentLocation.z = z;
  currentLocation.scale = scale;

  int displayed_y = getY(x, z);
  if (displayed_y >= 0)
  {
      currentLocation.y = displayed_y;
  }

  return currentLocation;
}

void MapView::setDimension(QString path, int scale) {
  if (scale > 0) {
    this->x *= this->scale;
    this->z *= this->scale;  // undo current scale transform
    this->scale = scale;
    this->x /= scale;  // and do the new scale transform
    this->z /= scale;
  } else {
    this->scale = 1;  // no scaling because no relation to overworld
    this->x = 0;  // and we jump to the center spawn automatically
    this->z = 0;
  }
  clearCache();
  cache->setPath(path);
}

void MapView::setDepth(int depth) {
  this->depth = depth;
}

void MapView::setFlags(int flags) {
  this->flags = flags;
}

int MapView::getFlags() const {
  return flags;
}

int MapView::getDepth() const {
  return depth;
}

void MapView::chunkUpdated(const QSharedPointer<Chunk>& chunk, int x, int z)
{
  ChunkID id(x, z);
  auto lock = renderCache.lock();
  auto& data = lock()[id];

  data.state.unset(RenderStateT::LoadingRequested);

  if (!chunk)
  {
    data.state.set(RenderStateT::Empty);
    data.state.unset(RenderStateT::RenderingRequested);
    return;
  }

  chunksToRedraw.enqueue(std::pair<ChunkID, QSharedPointer<Chunk>>(ChunkID(x, z), chunk));
  data.state.set(RenderStateT::RenderingRequested);

  if (havePendingToolTip && (id == pendingToolTipChunk))
  {
    havePendingToolTip = false;
    getToolTip_withChunkAvailable(pendingToolTipPos.x(), pendingToolTipPos.y(), chunk);
  }
}

QString MapView::getWorldPath() {
    return cache->getPath();
}

void MapView::updatePlayerPositions(const QVector<PlayerInfo> &playerList)
{
    currentPlayers.clear();
    for (auto info: playerList)
    {
        auto entity = QSharedPointer<Entity>::create(info);
        currentPlayers.push_back(entity);
    }
}

void MapView::updateSearchResultPositions(const QVector<QSharedPointer<OverlayItem>> &searchResults)
{
    currentSearchResults = searchResults;
}

void MapView::clearCache() {
  chunksToLoad.clear();
  chunksToRedraw.clear();
  renderCache.lock()().clear();
  cache->clear();
}

void MapView::mousePressEvent(QMouseEvent *event) {
  lastMousePressPosition = event->pos();
  dragging = true;
}

TopViewPosition MapCamera::transformPixelToBlockCoordinates(QPoint pixel_pos) const
{
  const QPoint centerPixel(size_pixels.width() / 2, size_pixels.height() / 2);
  const QPoint mouseDelta = pixel_pos - centerPixel;

  const QPointF mouseDeltaInBlocks = QPointF(mouseDelta) / zoom;

  const QPointF mousePosInBlocks = QPointF(centerpos_blocks.x, centerpos_blocks.z) + mouseDeltaInBlocks;

  return TopViewPosition(mousePosInBlocks.x(), mousePosInBlocks.y());
}

QPointF MapCamera::getPixelFromBlockCoordinates(TopViewPosition block_pos) const
{
  const QPointF centerBlock(centerpos_blocks.x, centerpos_blocks.z);
  const QPointF posBlock(block_pos.x, block_pos.z);

  const QPointF blockDelta = posBlock - centerBlock;
  const QPointF pixelDelta = blockDelta * zoom;

  const QPoint centerPixel(size_pixels.width() / 2, size_pixels.height() / 2);
  return centerPixel + pixelDelta;
}

void MapView::adjustZoom(double steps)
{
  const bool allowZoomOut = QSettings().value("zoomout", false).toBool();

  const double zoomMin = allowZoomOut ? 0.20 : 1.0;
  const double zoomMax = 20.0;

  const bool useFineZoomStrategy = QSettings().value("finezoom", false).toBool();

  if (useFineZoomStrategy) {
    zoom *= pow(1.3, steps);
  }
  else {
    zoom = floor(zoom + steps);
  }

  if (zoom < zoomMin) zoom = zoomMin;
  if (zoom > zoomMax) zoom = zoomMax;
}

uchar *MapView::getPlaceholder()
{
  static uchar placeholder[16 * 16 * 4];  // no chunk found placeholder
  static bool initDone = false;
  if (!initDone)
  {
    int offset = 0;
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        uchar color = ((x & 8) ^ (y & 8)) == 0 ? 0x44 : 0x88;
        placeholder[offset++] = color;
        placeholder[offset++] = color;
        placeholder[offset++] = color;
        placeholder[offset++] = 0xff;
      }
  }

  return placeholder;
}

void MapView::renderChunkAsync(const QSharedPointer<Chunk> &chunk)
{
    m_asyncRendererPool->enqueueJob([this, chunk](){
        ChunkRenderer::renderChunk(*this, chunk);
        QMetaObject::invokeMethod(this, [this, chunk](){
          renderingDone(chunk);
        });
    });
}

void MapView::renderingDone(const QSharedPointer<Chunk> &chunk)
{
    ChunkID id(chunk->chunkX, chunk->chunkZ);
    auto lock = renderCache.lock();

    auto& data = lock()[id];
    data.renderedChunk = chunk->rendered;
    data.state.unset(RenderStateT::RenderingRequested);

    const auto cgID = ChunkGroupID::fromCoordinates(id.getX(), id.getZ());
    auto cgLock = renderedChunkGroupsCache.lock();
    auto& grData = cgLock()[cgID];
    if (!grData.state.test(RenderStateT::RenderingRequested))
    {
      grData.state.set(RenderStateT::RenderingRequested);
      chunkGroupsToDraw.enqueue(cgID);
    }
}

void MapView::regularUpdate()
{
  if (!this->isEnabled()) {
    return;
  }

  regularUpdata__checkRedraw();

  const int maxIterLoadAndRender = 100000;

  {
    ChunkCache::Locker locker(*cache);

    {
      size_t i = 0;
      while (chunksToLoad.size() > 0)
      {
        const ChunkID id = chunksToLoad.dequeue();

        QSharedPointer<Chunk> chunk;
        locker.fetch(chunk, id, ChunkCache::FetchBehaviour::USE_CACHED_OR_UDPATE);
        i++;
        if (i > maxIterLoadAndRender)
        {
            break;
        }
      }
    }

    {
      size_t i = 0;
      while (chunksToRedraw.size() > 0)
      {
        const auto id = chunksToRedraw.dequeue();

        QSharedPointer<Chunk> chunk = id.second;
        if (!chunk)
        {
          locker.fetch(chunk, id.first, ChunkCache::FetchBehaviour::USE_CACHED_OR_UDPATE);
        }

        if (chunk)
        {
          renderChunkAsync(chunk);
          i++;
          if (i > maxIterLoadAndRender)
          {
              break;
          }
        }
      }
    }
  }

  regularUpdate__drawChunkGroups();

  redraw();
  update();
}

void MapView::regularUpdata__checkRedraw()
{
  const int maxIterLoadAndRender = 10000;
  int counter = 0;

  ChunkCache::Locker locker(*cache);
  auto renderdCacheLock = renderCache.lock();

  DrawHelper h(*this);

  for (int cz = h.startz; cz < h.startz + h.blockstall; cz++)
    for (int cx = h.startx; cx < h.startx + h.blockswide; cx++)
    {
      ChunkID id(cx, cz);

      bool need = false;
      RenderData& data = renderdCacheLock()[id];
      if ((!data.state[RenderStateT::Empty]) && (!data.state[RenderStateT::LoadingRequested]) && (!data.state[RenderStateT::RenderingRequested]))
      {
          need = (!data.renderedChunk) || redrawNeeded(*data.renderedChunk);
      }

      if (need)
      {
        chunksToRedraw.enqueue(std::pair<ChunkID, QSharedPointer<Chunk>>(id, nullptr));
        data.state.set(RenderStateT::RenderingRequested);
        counter++;

        if (counter > maxIterLoadAndRender)
        {
          return;
        }
      }
    }
}

void MapView::regularUpdate__drawChunkGroups()
{
  auto renderdCacheLock = renderCache.lock();
  auto renderdChunkGroupLock = renderedChunkGroupsCache.lock();

  while (chunkGroupsToDraw.size() > 0)
  {
    const auto cgid = chunkGroupsToDraw.dequeue();

    const auto topLeft = cgid.topLeft();
    QRectF groupRegionF(topLeft.x * 16, topLeft.z * 16, cgid.SIZE_N * 16, cgid.SIZE_N * 16);
    const auto groupRegionCenter = groupRegionF.center();
    QRect groupRegion = groupRegionF.toRect();
    DrawHelper h(groupRegionCenter.x(), groupRegionCenter.y(),
                 1.0, groupRegion);

    QImage image(groupRegion.size(), QImage::Format_RGB32);
    DrawHelper2 h2(h, image);

    for (size_t x = 0; x < cgid.SIZE_N; x++)
    {
      for (size_t z = 0; z < cgid.SIZE_N; z++)
      {
        const ChunkID cid(topLeft.x + x, topLeft.z + z);

        auto renderedChunkData = renderdCacheLock()[cid];

        h2.drawChunk_Map(cid.getX(), cid.getZ(), renderedChunkData.renderedChunk); // can deal with nullptr!
      }
    }

    auto& data = renderdChunkGroupLock()[cgid];
    data.state.unset(RenderStateT::RenderingRequested);
    data.renderedImg = image;
  }
}

void MapView::getToolTipMousePos(int mouse_x, int mouse_y)
{
    auto worldPos = getCamera().transformPixelToBlockCoordinates(QPoint(mouse_x, mouse_y)).floor();

    getToolTip(worldPos.x, worldPos.z);
}


void MapView::mouseMoveEvent(QMouseEvent *event) {

  getToolTipMousePos(event->x(), event->y());

  if (!dragging) {
    return;
  }
  x += (lastMousePressPosition.x()-event->x()) / zoom;
  z += (lastMousePressPosition.y()-event->y()) / zoom;
  lastMousePressPosition = event->pos();
}

void MapView::mouseReleaseEvent(QMouseEvent * event) {
  dragging = false;

  if (event->pos() == lastMousePressPosition)
  {
      // no movement of cursor -> assume normal click:
      getToolTipMousePos(event->x(), event->y());
  }
}

void MapView::mouseDoubleClickEvent(QMouseEvent *event) {

  const auto mouse_block = getCamera().transformPixelToBlockCoordinates(event->pos()).floor();

  // get the y coordinate
  int my = getY(mouse_block.x, mouse_block.z);

  QList<QVariant> properties;
  for (auto &item : getItems(mouse_block.x, my, mouse_block.z)) {
    properties.append(item->properties());
  }

  if (!properties.isEmpty()) {
    emit showProperties(properties);
  }
}

void MapView::wheelEvent(QWheelEvent *event) {
  if ((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier) {
    // change depth
    emit demandDepthChange(event->delta() / 120);
  } else {  // change zoom
    const double StepsOf15Degree = (event->angleDelta().y() / 120.0); // according to documentation of QWheelEvent
    adjustZoom( StepsOf15Degree );
  }
}

void MapView::keyPressEvent(QKeyEvent *event) {
  // default: 16 blocks / 1 chunk
  float stepSize = 16.0;

  if ((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier) {
    // 1 block for fine tuning
    stepSize = 1.0;
  }
  else if ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) {
    // 8 chunks
    stepSize = 128.0;
    if ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier) {
      // 32 chunks / 1 Region
      stepSize = 512.0;
    }
  }

  switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
      z -= stepSize / zoom;
      break;
    case Qt::Key_Down:
    case Qt::Key_S:
      z += stepSize / zoom;
      break;
    case Qt::Key_Left:
    case Qt::Key_A:
      x -= stepSize / zoom;
      break;
    case Qt::Key_Right:
    case Qt::Key_D:
      x += stepSize / zoom;
      break;
    case Qt::Key_PageUp:
    case Qt::Key_Q:
      adjustZoom(+1);
      break;
    case Qt::Key_PageDown:
    case Qt::Key_E:
      adjustZoom(-1);
      break;
    case Qt::Key_Home:
    case Qt::Key_Plus:
    case Qt::Key_BracketLeft:
      emit demandDepthChange(+1);
      break;
    case Qt::Key_End:
    case Qt::Key_Minus:
    case Qt::Key_BracketRight:
      emit demandDepthChange(-1);
      break;
  }
}

void MapView::resizeEvent(QResizeEvent *event) {
  imageChunks   = QImage(event->size(), QImage::Format_RGB32);
  //imageOverlays = QImage(event->size(), QImage::Format_RGBA8888);
  imageOverlays = QImage(event->size(), QImage::Format_ARGB32_Premultiplied);
  image_players = QImage(event->size(), QImage::Format_ARGB32_Premultiplied);
  if (!isEnabled())
  {
    redraw(); // to initialize buffers
  }
}

void MapView::paintEvent(QPaintEvent * /* event */) {
  QPainter p(this);
  p.drawImage(QPoint(0, 0), imageChunks);
  p.drawImage(QPoint(0, 0), imageOverlays);
  p.drawImage(QPoint(0, 0), image_players);
  p.end();
}

void DrawHelper3::drawChunkEntities(const RenderedChunk& rendered)
{
  if (!rendered.entities)
  {
    return;
  }

  for (const auto &type : m_parent.overlayItemTypes)
  {
    auto range = rendered.entities->equal_range(type);
    for (auto it = range.first; it != range.second; ++it) {
      // don't show entities above our depth
      int entityY = (*it)->midpoint().y;
      // everything below the current block,
      // but also inside the current block
      if (entityY < m_parent.depth + 1) {
        int entityX = static_cast<int>((*it)->midpoint().x) & 0x0f;
        int entityZ = static_cast<int>((*it)->midpoint().z) & 0x0f;
        int index = entityX + (entityZ << 4);
        int highY = rendered.depth[index];
        if ( (entityY+10 >= highY) ||
             (entityY+10 >= m_parent.depth) )
          (*it)->draw(h.x1, h.z1, m_parent.zoom, &canvas_entities);
      }
    }
  }
}

void MapView::redraw() {

  image_players.fill(0);
  imageOverlays.fill(0);

  if (!this->isEnabled()) {
    // blank
    imageChunks.fill(0xeeeeee);
    update();
    return;
  }

  ChunkCache::Locker locker(*cache);

  auto renderdCacheLock = renderedChunkGroupsCache.lock();

  DrawHelper h(*this);
  DrawHelper3 h2(h, *this);

  const auto camera = getCamera();

  const auto rootTopLeft = camera.transformPixelToBlockCoordinates(QPoint(0,0));
  const auto rootTopLeftChunkID = ChunkID::fromCoordinates(rootTopLeft.x, rootTopLeft.z);
  const auto rootTopLeftChunkGroupId = ChunkGroupID::fromCoordinates(rootTopLeftChunkID.getX(), rootTopLeftChunkID.getZ());

  const int chunkGroupsTall = 1 + (h.imageSize.height() / (ChunkID::SIZE_N * ChunkGroupID::SIZE_N));
  const int chunkGroupsWide = 1 + (h.imageSize.width() / (ChunkID::SIZE_N * ChunkGroupID::SIZE_N));

  for (int cz = h.startz; cz < h.startz + h.blockstall; cz += ChunkGroupID::SIZE_N)
    for (int cx = h.startx; cx < h.startx + h.blockswide; cx += ChunkGroupID::SIZE_N)
    {
      const auto cgID = ChunkGroupID::fromCoordinates(cx, cz);
      const auto topLeftChunk = ChunkID(cgID.topLeft().x, cgID.topLeft().z);

      RenderGroupData renderedData;
      auto it = renderdCacheLock().find(cgID);
      if (it != renderdCacheLock().end())
      {
        renderedData = *it;
      }

      const auto topLeftInBlocks = TopViewPosition(topLeftChunk.topLeft().x, topLeftChunk.topLeft().z);
      const auto topLeftInPixeln = camera.getPixelFromBlockCoordinates(topLeftInBlocks);
      const auto bottomRightInPixeln = camera.getPixelFromBlockCoordinates(
            TopViewPosition(topLeftInBlocks.x + 16 * ChunkGroupID::SIZE_N,
                            topLeftInBlocks.z + 16 * ChunkGroupID::SIZE_N));
      QRectF targetRect(topLeftInPixeln, bottomRightInPixeln);

      h2.getCanvas().drawImage(targetRect, renderedData.renderedImg);

      /*
       * drawChunk3(cx, cz, renderedData.renderedChunk, h2);

      if (false)
      {
        bool isCached = renderedData.state[RenderStateT::Empty];
        if (!isCached && renderedData.renderedChunk)
        {
          auto chunk = renderedData.renderedChunk->chunk.lock();
          isCached = (chunk != nullptr);
        }

        QBrush b(Qt::Dense6Pattern);
        b.setColor(renderedData.state[RenderStateT::RenderingRequested] ? Qt::yellow : (isCached ? Qt::green : Qt::red));
        if (b.color() != Qt::green)
        {
          TopViewPosition b1(cx*DrawHelper2::chunkSizeOrig, cz*DrawHelper2::chunkSizeOrig);
          TopViewPosition b2((cx+1)*DrawHelper2::chunkSizeOrig, (cz+1)*DrawHelper2::chunkSizeOrig);
          QRectF rect(camera.getPixelFromBlockCoordinates(b1), camera.getPixelFromBlockCoordinates(b2));
          h2.getCanvas().setBrush(b);
          h2.getCanvas().setPen(QPen(Qt::PenStyle::NoPen));
          h2.getCanvas().drawRect(rect);
        }
      }
        */
    }

  // add on the entity layer
  // done as part of drawChunk

  // draw the generated structures 
  for (auto &type : overlayItemTypes) {
    for (auto &item : overlayItems[type]) {
      if (item->intersects(OverlayItem::Point(h.x1 - 1, 0, h.z1 - 1),
                           OverlayItem::Point(h.x2 + 1, depth, h.z2 + 1))) {
        item->draw(h.x1, h.z1, zoom, &h2.getCanvas());
      }
    }
  }

  h2.getCanvas().setPen(QPen(Qt::PenStyle::SolidLine));

  const int maxViewWidth = 64 * 16; // (radius 32 chunks)

  const double firstGridLineX = ceil(h.x1 / maxViewWidth) * maxViewWidth;
  for (double x = firstGridLineX; x < h.x2; x += maxViewWidth)
  {
      const int line_x = round((x - h.x1) * zoom);
      h2.getCanvas().drawLine(line_x, 0, line_x, imageChunks.height());
      }

  const double firstGridLineZ = ceil(h.z1 / maxViewWidth) * maxViewWidth;
  for (double z = firstGridLineZ; z < h.z2; z += maxViewWidth)
  {
      const int line_z = round((z - h.z1) * zoom);
      h2.getCanvas().drawLine(0, line_z, imageChunks.width(), line_z);
  }

  h2.drawOverlayItemToPlayersCanvas(currentPlayers);
  h2.drawOverlayItemToPlayersCanvas(currentSearchResults);

  emit(coordinatesChanged(x, depth, z));

  update();
}

bool MapView::redrawNeeded(const RenderedChunk &renderedChunk) const
{
  return (renderedChunk.renderedAt != depth ||
          renderedChunk.renderedFlags != flags);
}

void MapView::drawChunk3(int x, int z, const QSharedPointer<RenderedChunk> &renderedChunk, DrawHelper3 &h)
{
    h.drawChunk_Map(x, z, renderedChunk);

    if (renderedChunk)
    {
      h.drawChunkEntities(*renderedChunk);
    }
}

void DrawHelper2::drawChunk_Map(int x, int z, const QSharedPointer<RenderedChunk>& renderedChunk) {

  // this figures out where on the screen this chunk should be drawn

  // and the center chunk screen coordinates
  double centerx = h.imageSize.width() / 2;
  double centery = h.imageSize.height() / 2;
  // which need to be shifted to account for panning inside that chunk
  centerx -= (h.x - centerchunkx * chunkSizeOrig) * h.zoom;
  centery -= (h.z - centerchunkz * chunkSizeOrig) * h.zoom;
  // centerx,y now points to the top left corner of the center chunk
  // so now calculate our x,y in relation
  double chunksize = chunkSizeOrig * h.zoom;
  centerx += (x - centerchunkx) * chunksize;
  centery += (z - centerchunkz) * chunksize;

  const uchar* srcImageData = renderedChunk ? renderedChunk->image : MapView::getPlaceholder();
  QImage srcImage(srcImageData, chunkSizeOrig, chunkSizeOrig, QImage::Format_RGB32);

  QRectF targetRect(centerx, centery, chunksize, chunksize);

  canvas.drawImage(targetRect, srcImage);
}

void MapView::getToolTip(int x, int z) {

  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);

  pendingToolTipPos = QPoint(x, z);
  pendingToolTipChunk = ChunkID(cx, cz);

  QSharedPointer<Chunk> chunk;
  bool chunkValid = false;
  {
    ChunkCache::Locker locked_cache(*cache);
    chunkValid = locked_cache.fetch(chunk, pendingToolTipChunk);
  }

  if (chunkValid)
  {
    getToolTip_withChunkAvailable(x, z, chunk);
  }

  havePendingToolTip = !chunkValid;
}

void MapView::getToolTip_withChunkAvailable(int x, int z, const QSharedPointer<Chunk>& chunk)
{
  int offset = (x & 0xf) + (z & 0xf) * 16;
  int y = 0;

  QString name  = "Unknown";
  QString biome = "Unknown Biome";
  QString blockstate;
  QString blockId = "?";
  QMap<QString, int> entityIds;

  if (chunk) {
    int top = qMin(depth, chunk->highest);
    for (y = top; y >= 0; y--) {
      int sec = y >> 4;
      ChunkSection *section = chunk->sections[sec];
      if (!section) {
        y = (sec << 4) - 1;  // skip entire section
        continue;
      }

      // get information about block
      const PaletteEntry & pdata = section->getPaletteEntry(offset, y);
      name = pdata.name;
      // in case of fully transparent blocks (meaning air)
      // -> we continue downwards
      auto & block = BlockIdentifier::Instance().getBlockInfo(pdata.hid);
      if (block.alpha == 0.0) continue;

      blockId = QString::number(pdata.hid) + "/" + QString::number(chunk->getBlockData(x,y,z).id);

      //block = chunk->getBlockData(x,y,z);

      //auto &blockInfo = blockDefinitions->getBlock(block.id, block.bd);
      //if (blockInfo.alpha == 0.0) continue;
      // list all Block States
      for (auto key : pdata.properties.keys()) {
        blockstate += key;
        blockstate += ":";
        blockstate += pdata.properties[key].toString();
        blockstate += " ";
      }
      blockstate.chop(1);
      break;
    }
    auto &bi = BiomeIdentifier::Instance().getBiome(chunk->biomes[(x & 0xf) + (z & 0xf) * 16]);
    biome = bi.name;

    // count Entity of each display type
    for (auto &item : getItems(x, y, z)) {
      entityIds[item->display()]++;
    }
  }

  QString entityStr;
  if (!entityIds.empty()) {
    QStringList entities;
    QMap<QString, int>::const_iterator it, itEnd = entityIds.cend();
    for (it = entityIds.cbegin(); it != itEnd; ++it) {
      if (it.value() > 1) {
        entities << it.key() + ":" + QString::number(it.value());
      } else {
        entities << it.key();
      }
    }
    entityStr = entities.join(", ");
  }

  QString hovertext = QString("X:%1 Y:%2 Z:%3 (Nether: X:%7 Z:%8) - %4 - %5 (id:%6)")
                              .arg(x).arg(y).arg(z)
                              .arg(biome)
                              .arg(name)
                              .arg(blockId)
                              .arg(x/8)
                              .arg(z/8);
  if (blockstate.length() > 0)
    hovertext += " (" + blockstate + ")";
  if (entityStr.length() > 0)
    hovertext += " - " + entityStr;

  hovertext += QString(" - zoom: %1").arg(zoom);

#ifdef DEBUG
  hovertext += " [Cache:"
            + QString().number(this->cache.getCost()) + "/"
            + QString().number(this->cache.getMaxCost()) + "]";
#endif

  emit hoverTextChanged(hovertext);
}

void MapView::addStructureFromChunk(QSharedPointer<GeneratedStructure> structure) {
  // update menu (if necessary)
  emit addOverlayItemType(structure->type(), structure->color());
  // add to list with overlays
  addOverlayItem(structure);
}

void MapView::addOverlayItem(QSharedPointer<OverlayItem> item) {
  // test if item is already in list
  for (auto &it: overlayItems[item->type()]) {
    OverlayItem::Point p1 = it  ->midpoint();
    OverlayItem::Point p2 = item->midpoint();
    if ( (p1.x == p2.x) && (p1.y == p2.y) && (p1.z == p2.z) )
      return;  // skip if already present
  }
  // otherwise add item
  overlayItems[item->type()].push_back(item);
}

void MapView::clearOverlayItems() {
  overlayItems.clear();
}

void MapView::setVisibleOverlayItemTypes(const QSet<QString>& itemTypes) {
  overlayItemTypes = itemTypes;
}

QList<QSharedPointer<OverlayItem> > MapView::getOverlayItems(const QString &type) const
{
    auto it = overlayItems.find(type);
    if (it != overlayItems.end())
    {
        return *it;
    }

    return QList<QSharedPointer<OverlayItem> >();
}

int MapView::getY(int x, int z) {
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  ChunkCache::Locker locked_cache(*cache);
  QSharedPointer<Chunk> chunk;
  locked_cache.fetch(chunk, ChunkID(cx, cz));
  return chunk ? (chunk->rendered ? chunk->rendered->depth[(x & 0xf) + (z & 0xf) * 16] : -1) : -1;
}

QList<QSharedPointer<OverlayItem>> MapView::getItems(int x, int y, int z) {
  QList<QSharedPointer<OverlayItem>> ret;
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  ChunkCache::Locker locked_cache(*cache);
  QSharedPointer<Chunk> chunk;
  locked_cache.fetch(chunk, ChunkID(cx, cz));

  if (chunk) {
    double invzoom = 10.0 / zoom;
    for (auto &type : overlayItemTypes) {
      // generated structures
      for (auto &item : overlayItems[type]) {
        double ymin = 0;
        double ymax = depth;
        if (item->intersects(OverlayItem::Point(x, ymin, z),
                             OverlayItem::Point(x, ymax, z))) {
          ret.append(item);
        }
      }

      // entities
      auto itemRange = chunk->entities->equal_range(type);
      for (auto itItem = itemRange.first; itItem != itemRange.second;
          ++itItem) {
        double ymin = y - 4;
        double ymax = depth + 4;

        if ((*itItem)->intersects(
            OverlayItem::Point(x - invzoom/2, ymin, z - invzoom/2),
            OverlayItem::Point(x + 1 + invzoom/2, ymax, z + 1 + invzoom/2))) {
          ret.append(*itItem);
        }
      }
    }
  }
  return ret;
}

MapCamera MapView::getCamera() const
{
  MapCamera camera;
  camera.centerpos_blocks.x = x;
  camera.centerpos_blocks.z = z;
  camera.size_pixels = QSize(width(), height());
  camera.zoom = zoom;
  return camera;
}
