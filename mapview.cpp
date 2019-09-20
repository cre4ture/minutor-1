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

const double g_zoomMin = 0.25;
const double g_zoomMax = 20.0;

class DrawHelper
{
private:
    MapView& parent;

public:
    const double zoom;

    int startx; // first chunk left
    int startz; // first chunk top

    int blockswide; // width in chunks
    int blockstall; // height in chunks

    double x1; // first coordinate left
    double z1; // first coordinate top
    double x2;
    double z2;

    DrawHelper(MapView& parent_)
        : parent(parent_)
        , zoom(parent.getZoom())
    {
        auto& image = parent.imageChunks;
        auto& x = parent.x;
        auto& z = parent.z;

        double chunksize = 16 * zoom;

        // first find the center block position
        int centerchunkx = floor(x / 16);
        int centerchunkz = floor(z / 16);
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
};

class DrawHelper2
{
public:
    DrawHelper2(DrawHelper& h_, MapView& parent)
                : h(h_)
                , canvas(&parent.imageChunks)
                , canvas_entities(&parent.imageOverlays)
                , canvas_players(&parent.image_players)
                , m_parent(parent)
    {}

    void drawChunkEntities(const Chunk &chunk);

    void drawChunk_Map(int x, int z, const QSharedPointer<Chunk> &chunk);

    void drawPlayers()
    {
        for (const auto& playerEntity: m_parent.currentPlayers)
        {
            playerEntity->draw(h.x1, h.z1, h.zoom, &canvas_players);
        }

        for (const auto& highlights: m_parent.currentSearchResults)
        {
            if (highlights != nullptr)
            {
                highlights->draw(h.x1, h.z1, h.zoom, &canvas_players);
            }
        }
    }

    QPainter& getCanvas() { return canvas; }

private:
    DrawHelper& h;
    QPainter canvas;
    QPainter canvas_entities;
    QPainter canvas_players;
    MapView& m_parent;
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
  , zoom_internal(1.0)
  , updateTimer()
  , dragging(false)
  , m_asyncRendererPool(threadpool)
{
    connect(this, SIGNAL(chunkRenderingCompleted(QSharedPointer<Chunk>)),
            this, SLOT(renderingDone(const QSharedPointer<Chunk>&)));

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

  int offset = 0;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uchar color = ((x & 8) ^ (y & 8)) == 0 ? 0x44 : 0x88;
      placeholder[offset++] = color;
      placeholder[offset++] = color;
      placeholder[offset++] = color;
      placeholder[offset++] = 0xff;
    }
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
  cache->clear();
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
  DrawHelper h(*this);
  DrawHelper2 h2(h, *this);

  drawChunk2(x, z, chunk, h2);

  update();
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
  cache->clear();
}

void MapView::mousePressEvent(QMouseEvent *event) {
  lastMousePressPosition = event->pos();
  dragging = true;
}

MapView::TopViewPosition MapView::transformMousePos(QPoint mouse_pos)
{
    double centerblockx = floor(this->x);
    double centerblockz = floor(this->z);

    int centerx = imageChunks.width() / 2;
    int centery = imageChunks.height() / 2;

    centerx -= (this->x - centerblockx) * getZoom();
    centery -= (this->z - centerblockz) * getZoom();

    return TopViewPosition(
                floor(centerblockx - (centerx - mouse_pos.x()) / getZoom()),
                floor(centerblockz - (centery - mouse_pos.y()) / getZoom())
                );
}

double MapView::getZoom() const
{
  return zoom_internal * zoom_internal;
}

void MapView::adjustZoom(double rate)
{
    zoom_internal *= (1.0 + (rate / 100.0));
    if (zoom_internal < g_zoomMin) zoom_internal = g_zoomMin;
    if (zoom_internal > g_zoomMax) zoom_internal = g_zoomMax;
}

void MapView::emit_chunkRenderingCompleted(const QSharedPointer<Chunk> &chunk)
{
    emit chunkRenderingCompleted(chunk);
}

void MapView::AsyncRenderLock::renderChunkAsync(const QSharedPointer<Chunk> &chunk)
{
    if (!chunk)
    return;

    ChunkID id(chunk->chunkX, chunk->chunkZ);

    {
        auto state = m_parent.renderStates[id];

        if (state == RENDERING)
        {
            return; // already busy with this chunk
        }

        m_parent.renderStates[id] = RENDERING;
    }

    m_parent.m_asyncRendererPool->enqueueJob([&parent=m_parent, chunk](){
        ChunkRenderer::renderChunk(parent, chunk.get());
        parent.emit_chunkRenderingCompleted(chunk);
    });
}

void MapView::renderingDone(const QSharedPointer<Chunk> &chunk)
{
    ChunkID id(chunk->chunkX, chunk->chunkZ);
    {
        QMutexLocker locker(&m_renderStatesMutex);
        renderStates[id] = NONE;
    }
}

void MapView::regularUpdate()
{
    const int maxIterLoadAndRender = 100000;

    {
        ChunkCache::Locker locker(*cache);

        {
            size_t i = 0;
            while (chunksToLoad.size() > 0)
            {
                const ChunkID id = *chunksToLoad.begin();
                chunksToLoad.erase(chunksToLoad.begin());

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
            AsyncRenderLock renderlock(*this);

            size_t i = 0;
            while (chunksToRedraw.size() > 0)
            {
                const ChunkID id = *chunksToRedraw.begin();
                chunksToRedraw.erase(chunksToRedraw.begin());

                QSharedPointer<Chunk> chunk;
                locker.fetch(chunk, id, ChunkCache::FetchBehaviour::USE_CACHED);
                renderlock.renderChunkAsync(chunk);
                i++;
                if (i > maxIterLoadAndRender)
                {
                    break;
                }
  }
        }
    }

  redraw();
    update();
}

void MapView::getToolTipMousePos(int mouse_x, int mouse_y)
{
    TopViewPosition worldPos =  transformMousePos(QPoint(mouse_x, mouse_y));

    getToolTip(worldPos.x, worldPos.z);
}


void MapView::mouseMoveEvent(QMouseEvent *event) {
  if (!dragging) {
    return;
  }
  x += (lastMousePressPosition.x()-event->x()) / getZoom();
  z += (lastMousePressPosition.y()-event->y()) / getZoom();
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
  int centerblockx = floor(this->x);
  int centerblockz = floor(this->z);

  int centerx = imageChunks.width() / 2;
  int centery = imageChunks.height() / 2;

  centerx -= (this->x - centerblockx) * getZoom();
  centery -= (this->z - centerblockz) * getZoom();

  int mx = floor(centerblockx - (centerx - event->x()) / getZoom());
  int mz = floor(centerblockz - (centery - event->y()) / getZoom());

  // get the y coordinate
  int my = getY(mx, mz);

  QList<QVariant> properties;
  for (auto &item : getItems(mx, my, mz)) {
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
    adjustZoom( event->delta() / 90.0 );
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
      z -= stepSize / getZoom();
      break;
    case Qt::Key_Down:
    case Qt::Key_S:
      z += stepSize / getZoom();
      break;
    case Qt::Key_Left:
    case Qt::Key_A:
      x -= stepSize / getZoom();
      break;
    case Qt::Key_Right:
    case Qt::Key_D:
      x += stepSize / getZoom();
      break;
    case Qt::Key_PageUp:
    case Qt::Key_Q:
      adjustZoom(1);
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
}

void MapView::paintEvent(QPaintEvent * /* event */) {
  QPainter p(this);
  p.drawImage(QPoint(0, 0), imageChunks);
  //p.drawImage(QPoint(0, 0), imageOverlays);
  p.drawImage(QPoint(0, 0), imageOverlays);
  p.drawImage(QPoint(0, 0), image_players);
  p.end();
}

void DrawHelper2::drawChunkEntities(const Chunk& chunk)
{
    for (const auto &type : m_parent.overlayItemTypes) {

        auto range = chunk.entities.equal_range(type);
        for (auto it = range.first; it != range.second; ++it) {
          // don't show entities above our depth
          int entityY = (*it)->midpoint().y;
          // everything below the current block,
          // but also inside the current block
          if (entityY < m_parent.depth + 1) {
            int entityX = static_cast<int>((*it)->midpoint().x) & 0x0f;
            int entityZ = static_cast<int>((*it)->midpoint().z) & 0x0f;
            int index = entityX + (entityZ << 4);
            int highY = chunk.depth[index];
            if ( (entityY+10 >= highY) ||
                 (entityY+10 >= m_parent.depth) )
              (*it)->draw(h.x1, h.z1, m_parent.getZoom(), &canvas_entities);
          }
        }
    }
}

void MapView::redraw() {
  if (!this->isEnabled()) {
    // blank
    imageChunks.fill(0xeeeeee);
    update();
    return;
  }

  image_players.fill(0);

  ChunkCache::Locker locker(*cache);

  DrawHelper h(*this);
  DrawHelper2 h2(h, *this);

  for (int cz = h.startz; cz < h.startz + h.blockstall; cz++)
    for (int cx = h.startx; cx < h.startx + h.blockswide; cx++)
      drawChunk(cx, cz, h2, locker);

  // clear the overlay layer
  imageOverlays.fill(0);

  // add on the entity layer
  // done as part of drawChunk

  // draw the generated structures 
  for (auto &type : overlayItemTypes) {
    for (auto &item : overlayItems[type]) {
      if (item->intersects(OverlayItem::Point(h.x1 - 1, 0, h.z1 - 1),
                           OverlayItem::Point(h.x2 + 1, depth, h.z2 + 1))) {
        item->draw(h.x1, h.z1, getZoom(), &h2.getCanvas());
      }
    }
  }

  const int maxViewWidth = 64 * 16; // (radius 32 chunks)

  const double firstGridLineX = ceil(h.x1 / maxViewWidth) * maxViewWidth;
  for (double x = firstGridLineX; x < h.x2; x += maxViewWidth)
  {
      const int line_x = round((x - h.x1) * getZoom());
      h2.getCanvas().drawLine(line_x, 0, line_x, imageChunks.height());
      }

  const double firstGridLineZ = ceil(h.z1 / maxViewWidth) * maxViewWidth;
  for (double z = firstGridLineZ; z < h.z2; z += maxViewWidth)
  {
      const int line_z = round((z - h.z1) * getZoom());
      h2.getCanvas().drawLine(0, line_z, imageChunks.width(), line_z);
  }

  h2.drawPlayers();

  emit(coordinatesChanged(x, depth, z));

  update();
}

void MapView::drawChunk(int x, int z, DrawHelper2& h, ChunkCache::Locker& locked_cache)
{
  if (!this->isEnabled())
    return;

  // fetch the chunk
    QSharedPointer<Chunk> chunk;
    const bool valid = locked_cache.fetch(chunk, ChunkID(x, z), ChunkCache::FetchBehaviour::USE_CACHED);
    if (!valid)
    {
        chunksToLoad.insert(ChunkID(x, z));
    }

    drawChunk2(x,z,chunk,h);
}

void MapView::drawChunk2(int x, int z, const QSharedPointer<Chunk> &chunk, DrawHelper2 &h)
{
  if (chunk && (chunk->renderedAt != depth ||
                  chunk->renderedFlags != flags))
    {
        chunksToRedraw.insert(ChunkID(x, z));
    return;
  }

    drawChunk3(x,z,chunk,h);
}

void MapView::drawChunk3(int x, int z, const QSharedPointer<Chunk> &chunk, DrawHelper2 &h)
{
    h.drawChunk_Map(x, z, chunk);

    if (chunk)
    {
        h.drawChunkEntities(*chunk);
    }
}

void DrawHelper2::drawChunk_Map(int x, int z, const QSharedPointer<Chunk>& chunk) {

  const int chunkSizeOrig = 16;

  // this figures out where on the screen this chunk should be drawn

  // first find the center chunk
  int centerchunkx = floor(m_parent.x / chunkSizeOrig);
  int centerchunkz = floor(m_parent.z / chunkSizeOrig);
  // and the center chunk screen coordinates
  double centerx = m_parent.imageChunks.width() / 2;
  double centery = m_parent.imageChunks.height() / 2;
  // which need to be shifted to account for panning inside that chunk
  centerx -= (m_parent.x - centerchunkx * chunkSizeOrig) * m_parent.getZoom();
  centery -= (m_parent.z - centerchunkz * chunkSizeOrig) * m_parent.getZoom();
  // centerx,y now points to the top left corner of the center chunk
  // so now calculate our x,y in relation
  double chunksize = chunkSizeOrig * m_parent.getZoom();
  centerx += (x - centerchunkx) * chunksize;
  centery += (z - centerchunkz) * chunksize;

  const uchar* srcImageData = chunk ? chunk->image : m_parent.placeholder;
  QImage srcImage(srcImageData, chunkSizeOrig, chunkSizeOrig, QImage::Format_RGB32);

  QRectF targetRect(centerx, centery, chunksize, chunksize);

  canvas.drawImage(targetRect, srcImage);
  }

void MapView::getToolTip(int x, int z) {
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);

  ChunkCache::Locker locked_cache(*cache);

  QSharedPointer<Chunk> chunk;
  const bool chunkValid = locked_cache.fetch(chunk, ChunkID(cx, cz));

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
  return chunk ? chunk->depth[(x & 0xf) + (z & 0xf) * 16] : -1;
}

QList<QSharedPointer<OverlayItem>> MapView::getItems(int x, int y, int z) {
  QList<QSharedPointer<OverlayItem>> ret;
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  ChunkCache::Locker locked_cache(*cache);
  QSharedPointer<Chunk> chunk;
  locked_cache.fetch(chunk, ChunkID(cx, cz));

  if (chunk) {
    double invzoom = 10.0 / getZoom();
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
      auto itemRange = chunk->entities.equal_range(type);
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
