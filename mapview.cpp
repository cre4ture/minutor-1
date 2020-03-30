/** Copyright (c) 2013, Sean Kasun */

#include "./mapview.h"
#include "./chunkcache.h"
#include "./chunkrenderer.h"
#include "./definitionmanager.h"
#include "./blockidentifier.h"
#include "./biomeidentifier.h"
#include "./clamp.h"
#include "./chunkrenderer.h"
#include "prioritythreadpool.h"

#include <QObject>
#include <QRunnable>
#include <QPainter>
#include <QResizeEvent>
#include <QMessageBox>
#include <assert.h>

static const double overscanZoomFactor = 0.8;

class DrawHelper
{
public:
  const MapCamera cam;
  const double x;
  const double z;
  const QSize imageSize;
  const double zoom;

  const double chunksize;

  const TopViewPosition topLeftBlock;
  const ChunkID topLeftChunk;

  double x1; // first coordinate left
  double z1; // first coordinate top
  double x2;
  double z2;

  DrawHelper(const double x_, const double z_, const double zoom_, const QSize size_)
    : cam(x_, z_, size_, zoom_)
    , x(x_)
    , z(z_)
    , imageSize(size_)
    , zoom(zoom_)
    , chunksize(16* zoom)
    // now calculate the topleft block on the screen
    , topLeftBlock(cam.transformPixelToBlockCoordinates(QPoint(0,0)))
    , topLeftChunk(ChunkID::fromCoordinates(topLeftBlock.x, topLeftBlock.z))
  {
    double halfviewwidth_blocks = cam.size_pixels.width() / 2 / zoom;
    double halvviewheight_blocks = cam.size_pixels.height() / 2 / zoom;
    x1 = x - halfviewwidth_blocks;
    z1 = z - halvviewheight_blocks;
    x2 = x + halfviewwidth_blocks;
    z2 = z + halvviewheight_blocks;
  }
};

class DrawHelper2
{
public:
  static const int chunkSizeOrig = 16;

  DrawHelper2(const DrawHelper& h_, QImage& imageBuffer)
    : h(h_)
    , canvas(&imageBuffer)
  {
    if (h.zoom < 1.0)
      canvas.setRenderHint(QPainter::SmoothPixmapTransform);
  }

  void drawChunk_Map(int x, int z, const QSharedPointer<RenderedChunk> &renderedChunk);
  void drawChunk_Map_int(int x, int z, const QImage& renderedChunk);

  QPainter& getCanvas() { return canvas; }

protected:
  const DrawHelper& h;
  QPainter canvas;
};



class DrawHelper3: public DrawHelper2
{
public:
  static const int chunkSizeOrig = 16;

  DrawHelper3(DrawHelper& h_,
              QImage& imageChunks,
              QImage& imageOverlays,
              QImage& imagePlayers)
    : DrawHelper2(h_, imageChunks)
    , canvas_entities(&imageOverlays)
    , canvas_players(&imagePlayers)
  {
  }

  void drawEntityMap(const Chunk::EntityMap& map, const ChunkGroupID &cgID, const RenderGroupData &depthImg, const QSet<QString> &overlayItemTypes, const int depth, const double zoom);

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
  QPainter canvas_entities;
  QPainter canvas_players;
};

class ChunkGroupDrawRegion
{
public:
  const MapCamera cam;

  const TopViewPosition rootTopLeft;
  const ChunkID rootTopLeftChunkID;
  const ChunkGroupID rootTopLeftChunkGroupId;

  const size_t chunkGroupsTall;
  const size_t chunkGroupsWide;

  ChunkGroupDrawRegion(const MapCamera& cam_)
    : cam(cam_)
    , rootTopLeft(cam.transformPixelToBlockCoordinates(QPoint(0,0)))
    , rootTopLeftChunkID(ChunkID::fromCoordinates(rootTopLeft.x, rootTopLeft.z))
    , rootTopLeftChunkGroupId(ChunkGroupID::fromCoordinates(rootTopLeftChunkID.getX(), rootTopLeftChunkID.getZ()))
    , chunkGroupsTall(2 + (static_cast<size_t>(cam_.size_pixels.height() / cam_.zoom / (ChunkID::SIZE_N * ChunkGroupID::SIZE_N))))
    , chunkGroupsWide(2 + (static_cast<size_t>(cam_.size_pixels.width() / cam_.zoom / (ChunkID::SIZE_N * ChunkGroupID::SIZE_N))))
  {}

  QRect getRect() const
  {
    return QRect(rootTopLeftChunkGroupId.getX(), rootTopLeftChunkGroupId.getZ(), chunkGroupsWide, chunkGroupsTall);
  }

  RectangleIterator begin() const
  {
    return RectangleIterator(getRect());
  }

  RectangleIterator end() const
  {
    return begin().end();
  }

  size_t count() const { return chunkGroupsWide * chunkGroupsTall;  }
};

class ChunkGroupRendererC: public ChunkGroupCamC
{
public:
  ChunkGroupRendererC(RenderGroupData& data_, const ChunkGroupID cgid_)
    : ChunkGroupCamC(data_.init(), cgid_)
    , ncdata(data_)
    , h(cam.centerpos_blocks.x, cam.centerpos_blocks.z,
        cam.zoom, cam.size_pixels)
    , image(ncdata.renderedImg)
    , imageDepth(ncdata.depthImg)
    , h2(h, image)
    , h2_depth(h, imageDepth)
  {
  }

  void drawChunk(const ChunkID cid, const QSharedPointer<RenderedChunk>& renderedChunk)
  {
    h2.drawChunk_Map(cid.getX(), cid.getZ(), renderedChunk); // can deal with nullptr!

    if (renderedChunk)
    {
      ncdata.entities[cid] = renderedChunk->entities;

      h2_depth.drawChunk_Map_int(cid.getX(), cid.getZ(), renderedChunk->depth);
    }
  }

private:
  RenderGroupData& ncdata;
  const DrawHelper h;
  QImage& image;
  QImage& imageDepth;
  DrawHelper2 h2;
  DrawHelper2 h2_depth;
};

MapView::MapView(const QSharedPointer<PriorityThreadPool> &threadpool__,
                 const QSharedPointer<ChunkCache>& chunkcache,
                 QWidget *parent)
  : QWidget(parent)
  , depth(255)
  , scale(1)      // overworld coordinate mapping
  , zoomIndex(0)  // 1:1
  , updateTimer()
  , cache(chunkcache)
  , renderedChunkGroupsCache(std::make_unique<RenderedChunkGroupCacheUnprotectedT>("rendergroups"))
  , dragging(false)
  , threadpool_(threadpool__)
  , invoker()
  , hasChanged(true)
  , updateChecker()
  , safeThreadPoolI(*threadpool_)
{
  havePendingToolTip = false;

  connect(&updateTimer, SIGNAL(timeout()),
          this, SLOT(regularUpdate()));

  updateTimer.setInterval(30);
  updateTimer.start();

  connect(cache.data(), SIGNAL(structureFound(QSharedPointer<GeneratedStructure>)),
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

  qRegisterMetaType<QSharedPointer<RenderedChunk> >("QSharedPointer<RenderedChunk>");
}

MapView::~MapView()
{

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

  connect(cache.data(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&, int, int)),
          this, SLOT(chunkUpdated(const QSharedPointer<Chunk>&, int, int)));
}

void MapView::setLocation(double x, double z) {
  setLocation(x, depth, z, false, true);
}

void MapView::setLocation(double x, int y, double z, bool ignoreScale, bool useHeight) {
  changed();
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

MapView::BlockLocation MapView::getConfiguredLocation()
{
  MapView::BlockLocation loc;
  loc.x = x;
  loc.y = depth;
  loc.z = z;
  loc.scale = scale;
  return loc;
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

  cache->setPath(path);
  clearCache();
}

void MapView::setDepth(int depth) {
  this->depth = depth;
  changed();
}

void MapView::setFlags(int flags) {
  this->flags = flags;
  changed();
}

int MapView::getFlags() const {
  return flags;
}

int MapView::getDepth() const {
  return depth;
}

void MapView::chunkUpdated(const QSharedPointer<Chunk>& chunk, int x, int z)
{
  if (!chunk)
  {
    return;
  }

  const ChunkID cid(x, z);
  const ChunkGroupID cgid = ChunkGroupID::fromCoordinates(x, z);

  {
    auto lock = renderedChunkGroupsCache.lock();
    if (!lock().contains(cgid))
    {
      return;
    }
  }

  /*
  ChunkGroupDrawRegion cgit(getCamera());
  const bool visible = (!cgit.getRect().contains(cgid.getX(), cgid.getZ()));
  */

  renderChunkAsync(chunk);

  if (havePendingToolTip && (cid == pendingToolTipChunk))
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

  changed();
}

void MapView::updateSearchResultPositions(const QVector<QSharedPointer<OverlayItem>> &searchResults)
{
  currentSearchResults = searchResults;
}

void MapView::clearCache()
{
  const bool backgroundWasEnabled = (updateChecker != nullptr);
  setBackgroundActivitiesEnabled(false);

  cache->clear();
  renderedChunkGroupsCache.lock()().clear();

  setBackgroundActivitiesEnabled(backgroundWasEnabled);
}

void MapView::adjustZoom(double steps, bool allowZoomOut)
{
  changed();

  int sign = (steps < 0) ? -1 : 1;
  zoomIndex += ceil(fabs(steps)) * sign;

  // use Fibonacci numbers to get natural zoom behaviour
  const float zoomTable[] = {1, 2, 3, 5, 8, 13, 21, 34, 55, 89};
  const int zoomMin = allowZoomOut ? -4 : 0;
  const int zoomMax = (sizeof(zoomTable) / sizeof(float)) -1;

  if (zoomIndex < zoomMin) zoomIndex = zoomMin;
  if (zoomIndex > zoomMax) zoomIndex = zoomMax;

  // apply new zoom
  if (zoomIndex < 0)
    zoom = 1 / pow(2.0, -zoomIndex);
  else
    zoom = zoomTable[zoomIndex];
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

void MapView::changed()
{
  hasChanged = true;
  if (updateChecker)
  {
    updateChecker->update();
  }
}

const QImage& getPlaceholder()
{
  static uchar placeholder[16 * 16 * 4];  // no chunk found placeholder
  static bool initDone = false;
  static QImage img;
  if (!initDone)
  {
    initDone = true;

    int offset = 0;
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++) {
        uchar color = ((x & 8) ^ (y & 8)) == 0 ? 0x44 : 0x88;
        placeholder[offset++] = color;
        placeholder[offset++] = color;
        placeholder[offset++] = color;
        placeholder[offset++] = 0xff;
      }

    img = QImage(placeholder, ChunkID::SIZE_N, ChunkID::SIZE_N, QImage::Format_RGB32);
  }

  return img;
}

const QImage& getChunkGroupPlaceholder()
{
  static QImage img;
  static bool initDone = false;

  if (!initDone)
  {
    initDone = true;

    const QImage& placeholderImage = getPlaceholder();

    img = QImage(ChunkGroupID::getSize(), QImage::Format_RGB32);
    QPainter canvas(&img);

    for (int iy = 0; iy < ChunkGroupID::SIZE_N; iy++)
    {
      for (int ix = 0; ix < ChunkGroupID::SIZE_N; ix++)
      {
        const QRect targetRect(ix * ChunkID::SIZE_N, iy * ChunkID::SIZE_N, ChunkID::SIZE_N, ChunkID::SIZE_N);
        canvas.drawImage(targetRect, placeholderImage);
      }
    }
  }

  return img;
}

size_t MapView::renderChunkAsync(const QSharedPointer<Chunk> &chunk)
{
  return safeThreadPoolI.enqueueJob([this, chunk](const ExecutionGuard& guard){

    renderChunkSync(chunk, guard);

  }, JobPrio::high);
}

void MapView::renderChunkSync(const QSharedPointer<Chunk> &chunk, const ExecutionGuard& guard)
{
  auto renderedChunk = QSharedPointer<RenderedChunk>::create(chunk);
  renderedChunk->init();

  ChunkRenderer::renderChunk(*this, chunk, *renderedChunk);

  m_invoker.invokeCancellable(guard.getStatusToken(), [this, renderedChunk](const ExecutionGuard& guard){
    renderingDone(renderedChunk);
  });
}

void MapView::updateCacheSize(bool onlyIncrease)
{
  DrawHelper h(x, z, zoom * overscanZoomFactor, imageChunks.size());
  ChunkGroupDrawRegion region(h.cam);

  const int newCount = region.count() * 3;

  auto lock = renderedChunkGroupsCache.lock();

  if (onlyIncrease && (newCount < lock().maxCost()))
  {
    return;
  }

  lock().setMaxCost(newCount);
}

void MapView::changeEvent(QEvent *e)
{
  if (e && (e->type() & QEvent::EnabledChange))
  {
    if (!isEnabled())
    {
      setBackgroundActivitiesEnabled(false);
      clearCache(); // make sure cache is cleared before enable again!
    }
  }
}

void MapView::setBackgroundActivitiesEnabled(bool enabled)
{
  if ((!updateChecker) && enabled)
  {
    updateChecker = std::make_shared<UpdateChecker>(*this, cache, threadpool_, [this](QSharedPointer<Chunk> chunk, const ExecutionGuard &guard){
        renderChunkSync(chunk, guard);
    });

    changed();
  }

  if (!enabled)
  {
    updateChecker.reset();
  }
}

void MapView::renderingDone(const QSharedPointer<RenderedChunk> renderedChunk)
{
  if (!renderedChunk)
  {
    return;
  }

  changed();

  ChunkID id(renderedChunk->chunkX, renderedChunk->chunkZ);

  const auto cgID = ChunkGroupID::fromCoordinates(id.getX(), id.getZ());
  {
    QSharedPointer<RenderGroupData> grData;
    {
      auto cgLock = renderedChunkGroupsCache.lock();
      grData = cgLock().findOrCreate(cgID);
    }

    if (grData && renderedChunk)
    {
      {
        ChunkGroupRendererC renderer(*grData, cgID);
        renderer.drawChunk(id, renderedChunk);
      }

      renderedChunk->freeImageData();

      auto& state = grData->states[id];
      state.flags.unset(RenderStateT::RenderingRequested);
      state.renderedFor = renderedChunk->renderedFor;

      grData->renderedFor.invalidate();
    }
  }
}

void MapView::regularUpdate()
{
  if (!this->isEnabled()) {
    return;
  }

  if (!hasChanged)
  {
    return;
  }

  hasChanged = false;

  updateCacheSize(true);

  redraw();
  update();

  updateCacheSize(false);
}

MapView::UpdateChecker::UpdateChecker(MapView& parent_,
                                      const QSharedPointer<ChunkCache> &cache_,
                                      const QSharedPointer<PriorityThreadPool>& threadpool_,
                                      const RenderFuncT& renderChunkRequestFunction_)
  : parent(parent_)
  , mutex()
  , cache(cache_)
  , threadpool(threadpool_)
  , renderChunkFunction(renderChunkRequestFunction_)
  , autoPerformance(std::chrono::milliseconds(30))
  , updateIsRunning(false)
  , isIdleJobRegistered(false)
  , asyncGuard()
{
  idleJob = std::make_shared<std::function<void()> >([this, cancelToken = asyncGuard.getTokenPtr()](){
    auto guard = cancelToken.createExecutionGuardChecked();
    idleJobFunction();
  });
}

void MapView::UpdateChecker::idleJobFunction()
{
  ChunkID id;

  {
    std::unique_lock<std::mutex> guard(mutex);

    if (!updateIsRunning)
    {
      size_t len = chunksToRedraw.getCurrentQueueLength();
      //std::cout << len << std::endl;
      if (len < getCurrentQueueLimit() / 2)
      {
        updateIsRunning = true;
        guard.unlock();

        update_internal(true);

        guard.lock();
        updateIsRunning = false;
      }
    }

    if (!chunksToRedraw.tryPop(id))
    {
      unregisterIdleJobIfNotJetDone();
      return;
    }
  }

  QSharedPointer<Chunk> chunk = cache->getChunkSynchronously(id);

  if (chunk)
  {
    renderChunkFunction(chunk, asyncGuard.getTokenPtr().createExecutionGuardChecked());
  }
}

void MapView::UpdateChecker::registerIdleJobOfNotYetDone()
{
  if (!isIdleJobRegistered)
  {
    threadpool->queueSink().registerDefaultValue(idleJob, JobPrio::idle);
    isIdleJobRegistered = true;
  }
}

void MapView::UpdateChecker::unregisterIdleJobIfNotJetDone()
{
  if (isIdleJobRegistered)
  {
    threadpool->queueSink().unregisterDefaultValue(idleJob);
    isIdleJobRegistered = false;
  }
}

MapView::UpdateChecker::~UpdateChecker()
{
  unregisterIdleJobIfNotJetDone();
}

void MapView::UpdateChecker::update()
{
  std::unique_lock<std::mutex> guard(mutex);

  if (!updateIsRunning)
  {
    update_internal(false);
  }
}

void MapView::UpdateChecker::update_internal(bool regular)
{
  regularUpdata__checkRedraw();

  if (chunksToRedraw.getCurrentQueueLength() != 0)
  {
    if (regular)
    {
      const auto lastDuration = std::chrono::duration_cast<std::chrono::milliseconds>(autoPerformanceTimer.updateTime());
      //std::cout << "ld: " << lastDuration.count() << std::endl;
      autoPerformance.update(lastDuration);
    }

    registerIdleJobOfNotYetDone();

    //std::cout << "ctr: " << chunksToRedraw.getCurrentQueueLength() << std::endl;
  }
}

namespace {
  const size_t numberOfCpuCores = std::thread::hardware_concurrency();
}

size_t MapView::UpdateChecker::getCurrentQueueLimit() const
{
  return numberOfCpuCores * 4;
}

void MapView::UpdateChecker::regularUpdata__checkRedraw()
{
  const size_t currentQueueLen = chunksToRedraw.getCurrentQueueLength();
  if (currentQueueLen > getCurrentQueueLimit())
  {
    return;
  }

  DrawHelper h(parent.x, parent.z, parent.zoom * overscanZoomFactor, parent.imageChunks.size());

  ChunkGroupDrawRegion cgit(h.cam);

  chunkRedrawIterator.setRange(cgit.chunkGroupsWide, cgit.chunkGroupsTall, true);
  const int maxIters = cgit.chunkGroupsWide * cgit.chunkGroupsTall;

  for (int i = 0; i < maxIters; i++)
  {
    std::pair<int, int> id = chunkRedrawIterator.getNext(cgit.rootTopLeftChunkGroupId.getX(), cgit.rootTopLeftChunkGroupId.getZ());
    ChunkGroupID cgid(id.first, id.second);

    auto data = parent.renderedChunkGroupsCache.lock()().findOrCreate(cgid);
    if (data)
    {
      if (parent.redrawNeeded(*data))
      {
        regularUpdata__checkRedraw_chunkGroup(cgid, *data);

        if (chunksToRedraw.getCurrentQueueLength() > getCurrentQueueLimit())
        {
          return;
        }
      }
    }
  }
}

void MapView::UpdateChecker::regularUpdata__checkRedraw_chunkGroup(const ChunkGroupID &cgid, RenderGroupData &data)
{  
  data.renderedFor = parent.getCurrentRenderParams();

  for(auto coordinate : cgid)
  {
    const ChunkID cid(coordinate.getX(), coordinate.getZ());
    auto& state = data.states[cid];

    if (state.flags[RenderStateT::Empty] || state.flags[RenderStateT::LoadingRequested] || state.flags[RenderStateT::RenderingRequested])
    {
      continue;
    }

    if (parent.redrawNeeded(state))
    {
      state.flags.set(RenderStateT::RenderingRequested);
      chunksToRedraw.push(cid);
    }
  }
}

MapCamera CreateCameraForChunkGroup(const ChunkGroupID& cgid)
{
  const auto topLeft = cgid.topLeft();
  QRectF groupRegionF(topLeft.getX() * 16, topLeft.getZ() * 16, cgid.SIZE_N * 16, cgid.SIZE_N * 16);
  const auto groupRegionCenter = groupRegionF.center();
  QRect groupRegion = groupRegionF.toRect();

  return MapCamera(groupRegionCenter.x(),
                   groupRegionCenter.y(),
                   groupRegion.size(),
                   1.0);
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

  changed();
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
  Qt::KeyboardModifier modifier4DepthSlider = Qt::KeyboardModifier(QSettings().value("modifier4DepthSlider", Qt::ShiftModifier).toUInt());
  Qt::KeyboardModifier modifier4ZoomOut     = Qt::KeyboardModifier(QSettings().value("modifier4ZoomOut",     Qt::ControlModifier).toUInt());

  if ((event->modifiers() & modifier4DepthSlider) == modifier4DepthSlider) {
    // change depth
    emit demandDepthChange(event->delta() / 120);
  } else if ((event->modifiers() & modifier4ZoomOut) == modifier4ZoomOut) {
    // allow change zoom also to zoom OUT
    adjustZoom( event->delta() / 120.0, true );
    redraw();
  } else {
    // normal change zoom
    adjustZoom( event->delta() / 120.0, false );
    redraw();
  }
}

void MapView::keyPressEvent(QKeyEvent *event) {
  // default: 16 blocks / 1 chunk
  float stepSize = 16.0;
  bool allowZoomOut = false;

  if        ((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier) {
    // 1 block for fine tuning
    stepSize = 1.0;
  } else if ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) {
    // 8 chunks
    stepSize = 128.0;
  } else if ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier) {
    // 32 chunks / 1 Region
    stepSize = 512.0;
    allowZoomOut = true;
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
      adjustZoom(1, allowZoomOut);
      break;
    case Qt::Key_PageDown:
    case Qt::Key_E:
      adjustZoom(-1, allowZoomOut);
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
  changed();

  // adapt size of rendered images
  imageChunks   = QImage(event->size(), QImage::Format_RGB32);
  //imageOverlays = QImage(event->size(), QImage::Format_RGBA8888);
  imageOverlays = QImage(event->size(), QImage::Format_ARGB32_Premultiplied);
  image_players = QImage(event->size(), QImage::Format_ARGB32_Premultiplied);

  // restrict zoom and adapt ChunkCache
  adjustZoom(0, true);
  // redraw everything
  redraw();
}

void MapView::paintEvent(QPaintEvent * /* event */) {
  QPainter p(this);
  p.drawImage(QPoint(0, 0), imageChunks);
  p.drawImage(QPoint(0, 0), imageOverlays);
  p.drawImage(QPoint(0, 0), image_players);
  p.end();
}

enum {
  BELOW_GROUND_VALUE = 10
};

void DrawHelper3::drawEntityMap(const Chunk::EntityMap &map,
                                const ChunkGroupID& cgID,
                                const RenderGroupData &renderedData,
                                const QSet<QString>& overlayItemTypes,
                                const int depth,
                                const double zoom)
{
  ChunkGroupCamC cam(renderedData, cgID);

  for (const auto &type : overlayItemTypes)
  {
    auto range = const_cast<Chunk::EntityMap&>(map).equal_range(type);
    for (auto it = range.first; it != range.second; ++it) {
      const auto midpoint = (*it)->midpoint();
      // don't show entities above our depth
      // everything below the current block,
      // but also inside the current block
      if (midpoint.y < depth + 1) {

        const int highY = cam.getHeightAt(TopViewPosition(midpoint.x, midpoint.z));

        if ( (midpoint.y+BELOW_GROUND_VALUE >= highY) ||
             (midpoint.y+BELOW_GROUND_VALUE >= depth) )
          (*it)->draw(h.x1, h.z1, zoom, &canvas_entities);
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

  setBackgroundActivitiesEnabled(true);

  const bool chunkgroupstatus = QSettings().value("chunkgroupstatus", false).toBool();
  const bool chunkCacheSatus = QSettings().value("chunkcachestatus", false).toBool();

  DrawHelper h(x,z,zoom,imageChunks.size());
  DrawHelper3 h2(h, imageChunks, imageOverlays, image_players);

  ChunkGroupDrawRegion cgit(h.cam);

  QImage placeholderImg = getChunkGroupPlaceholder().copy();

  auto renderedDataDummy = QSharedPointer<RenderGroupData>::create();
  renderedDataDummy->renderedImg = placeholderImg.copy();

  if (chunkgroupstatus)
  {
    renderedDataDummy->renderedImg.fill(Qt::red);
    placeholderImg.fill(Qt::green);
            }

  ChunkGroupIDListT cgidList;
  cgidList.reserve(cgit.count());

  {
    auto renderdCacheLock = renderedChunkGroupsCache.lock();
    for (auto point: cgit)
    {
      const auto cgid = ChunkGroupID(point.getX(), point.getZ());

      auto it = renderdCacheLock()[cgid];

      QSharedPointer<RenderGroupData> renderedData = it ? it : renderedDataDummy;

      cgidList.push_back(std::pair<ChunkGroupID, QSharedPointer<RenderGroupData> >(cgid, renderedData));
          }
        }

  redraw_drawMap(cgidList, placeholderImg, h.cam, h2);

  if (chunkCacheSatus)
  {
    redraw_drawChunkCacheStatus(cgidList, renderedDataDummy, h2.getCanvas(), h.cam);
      }

  // add on the entity layer
  // done as part of drawChunk

  const OverlayItem::Point p1(h.x1 - 1, 0, h.z1 - 1);
  const OverlayItem::Point p2(h.x2 + 1, depth, h.z2 + 1);

  // draw the generated structures
  for (auto &type : overlayItemTypes) {
    for (auto &item : overlayItems[type]) {
      if (item->intersects(p1, p2)) {
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


void MapView::redraw_drawMap(const MapView::ChunkGroupIDListT &cgidList,
                             const QImage& placeholderImg,
                             const MapCamera &camera,
                             DrawHelper3 &h2)
{
  const bool displayDepthMap = QSettings().value("depthmapview", false).toBool();

  for (const auto& itp: cgidList)
  {
    const ChunkGroupID cgid = itp.first;
    const RenderGroupData& renderedData = *itp.second;

    const auto topLeftChunk = ChunkID(cgid.topLeft().getX(), cgid.topLeft().getZ());
    const auto topLeftInBlocks = TopViewPosition(topLeftChunk.topLeft().getX(), topLeftChunk.topLeft().getZ());
    const auto topLeftInPixeln = camera.getPixelFromBlockCoordinates(topLeftInBlocks);
    const auto bottomRightInPixeln = camera.getPixelFromBlockCoordinates(
          TopViewPosition(topLeftInBlocks.x + 16 * ChunkGroupID::SIZE_N,
                          topLeftInBlocks.z + 16 * ChunkGroupID::SIZE_N));
    QRectF targetRect(topLeftInPixeln, bottomRightInPixeln);

    const QImage& imageToDraw = displayDepthMap ? renderedData.depthImg : renderedData.renderedImg;

    h2.getCanvas().drawImage(targetRect, imageToDraw.isNull() ? placeholderImg : imageToDraw);

    for (const auto& entityMap: renderedData.entities)
    {
      if (entityMap)
      {
        h2.drawEntityMap(*entityMap, cgid, renderedData, overlayItemTypes, depth, zoom);
      }
    }
  }
}

void MapView::redraw_drawChunkCacheStatus(const ChunkGroupIDListT &cgidList,
                                          const QSharedPointer<RenderGroupData>& renderedDataDummy,
                                          QPainter& canvas,
                                          const MapCamera& camera)
{
  QBrush b(Qt::Dense6Pattern);
  canvas.setPen(QPen(Qt::PenStyle::NoPen));

  ChunkCache::Locker locker(*cache);

  for (const auto& itp: cgidList)
  {
    const ChunkGroupID cgid = itp.first;
    const RenderGroupData& renderedData = *itp.second;

    const bool isActuallyChunkDataAvailable = ((itp.second != renderedDataDummy) && !renderedData.renderedImg.isNull());
    if (isActuallyChunkDataAvailable)
    {
      for(auto coordinate : cgid)
      {
        const ChunkID cid(coordinate.getX(), coordinate.getZ());
        const bool isCached = locker.isCached(cid);
        b.setColor(isCached ? Qt::green : Qt::red);

        CoordinateID b1 = cid.topLeft();
        CoordinateID b2 = cid.bottomRight();
        QRectF rect(camera.getPixelFromBlockCoordinates(TopViewPosition(b1.getX(), b1.getZ())),
                    camera.getPixelFromBlockCoordinates(TopViewPosition(b2.getX(), b2.getZ())));

        canvas.setBrush(b);
        canvas.drawRect(rect);
      }
    }
  }
}

void DrawHelper2::drawChunk_Map(int x, int z, const QSharedPointer<RenderedChunk>& renderedChunk) {

  const QImage& srcImage = renderedChunk ? renderedChunk->image : getPlaceholder();
  drawChunk_Map_int(x, z, srcImage);
}

void DrawHelper2::drawChunk_Map_int(int x, int z, const QImage &srcImage)
{
  double chunksize = chunkSizeOrig * h.zoom;

  // this figures out where on the screen this chunk should be drawn
  const auto topLeft = ChunkID(x, z).topLeft();
  const QPointF topLeftInPixel = h.cam.getPixelFromBlockCoordinates(TopViewPosition(topLeft.getX(), topLeft.getZ()));

  QRectF targetRect(topLeftInPixel, QSizeF(chunksize, chunksize));

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
    chunkValid = locked_cache.fetch(chunk, pendingToolTipChunk, JobPrio::high);
  }

  if (chunkValid)
  {
    getToolTip_withChunkAvailable(x, z, chunk);
  }
  else
  {
    QString hovertext = QString("X:%1 Z:%3 - data not cached -")
        .arg(x).arg(z);

    emit hoverTextChanged(hovertext);
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
      int section_idx = y >> 4;
      const ChunkSection *section = chunk->sections[section_idx];
      if (!section) {
        y = (section_idx << 4) - 1;  // skip entire section
        continue;
      }

      // get information about block
      const PaletteEntry & pdata = section->getPaletteEntry(offset, y);
      name = pdata.name;
      // in case of fully transparent blocks (meaning air)
      // -> we continue downwards
      auto & block = BlockIdentifier::Instance().getBlockInfo(pdata.hid);
      if (block.alpha == 0.0) continue;

      if (flags & MapView::flgSeaGround && block.isLiquid()) continue;

      blockId = QString::number(pdata.hid)
          + "/" + QString::number(chunk->getBlockData(x,y,z).id); // only to check if implementation is correct

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
    int biome_code = chunk->get_biome((x & 0xf), y, (z & 0xf));
    auto &bi = BiomeIdentifier::Instance().getBiome(biome_code);
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

#if defined(DEBUG) || defined(_DEBUG) || defined(QT_DEBUG)
  hovertext += " [Cache:"
            + QString().number(this->cache->getCacheUsage()) + "/"
            + QString().number(this->cache->getCacheMax()) + "]";
  hovertext += " Zoom:" + QString().number(zoomIndex);
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

  changed();
}

void MapView::clearOverlayItems() {
  overlayItems.clear();
  changed();
}

void MapView::setVisibleOverlayItemTypes(const QSet<QString>& itemTypes) {
  overlayItemTypes = itemTypes;
  changed();
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

  ChunkID cid = ChunkID::fromCoordinates(x, z);
  ChunkGroupID cgid = ChunkGroupID::fromCoordinates(cid.getX(), cid.getZ());

  auto lock = renderedChunkGroupsCache.lock();

  auto it = lock()[cgid];
  if (!it)
  {
    return -1;
  }

  ChunkGroupCamC cam(*it, cgid);

  return cam.getHeightAt(TopViewPosition(x, z));
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
        double ymin = y - BELOW_GROUND_VALUE;
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

RenderGroupData::RenderGroupData()
  : renderedImg()
  , depthImg()
{
  clear();
}

void RenderGroupData::clear()
{
  entities.clear();
  renderedImg = QImage();
  depthImg = QImage();
  renderedFor = RenderParams();
}

RenderGroupData& RenderGroupData::init()
{
  if (renderedImg.isNull())
  {
    renderedImg = getChunkGroupPlaceholder().copy();
    depthImg = QImage(ChunkGroupID::getSize(), QImage::Format_Grayscale8);
    depthImg.fill(0);
  }
  return *this;
}


MapView::AsyncLoop::AsyncLoop(const QSharedPointer<PriorityThreadPool>& threadpool_, const JobPrio prio_,
                              const std::function<void ()> &func_)
  : threadpool(threadpool_)
  , prio(prio_)
  , func(func_)
  , safeThreadI(*threadpool)
{
  requestNext();
}

void MapView::AsyncLoop::requestNext()
{
  safeThreadI.enqueueJob([this](const ExecutionGuard& guard){
    func();
    requestNext();
  }, prio);
}
