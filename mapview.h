/** Copyright (c) 2013, Sean Kasun */
#ifndef MAPVIEW_H_
#define MAPVIEW_H_

#include "./chunkcache.h"
#include "./playerinfos.h"
#include "./mapcamera.hpp"

#include "./lockguarded.hpp"
#include "./enumbitset.hpp"
#include "./mapviewrenderer.h"
#include "./safeinvoker.h"
#include "./threadsafequeue.hpp"

#include <QtWidgets/QWidget>
#include <QSharedPointer>

#include <QVector>
#include <unordered_set>

class DefinitionManager;
class BiomeIdentifier;
class BlockIdentifier;
class OverlayItem;
class DrawHelper;
class DrawHelper2;
class DrawHelper3;
class ChunkRenderer;
class PriorityThreadPool;

class MapView : public QWidget {
  Q_OBJECT

    friend class ChunkRenderer;

 public:
  /// Values for the individual flags
  enum {
    flgLighting     = 1 << 0,
    flgMobSpawn     = 1 << 1,
    flgCaveMode     = 1 << 2,
    flgDepthShading = 1 << 3,
    flgBiomeColors  = 1 << 4,
    flgSeaGround    = 1 << 5,
    flgSingleLayer  = 1 << 6
  };

  typedef struct {
    float x, y, z;
    int scale;

    QVector3D getPos3D() const
    {
        return QVector3D(x,y,z);
    }

  } BlockLocation;

  explicit MapView(const QSharedPointer<PriorityThreadPool>& threadpool,
                   const QSharedPointer<ChunkCache> &chunkcache,
                   QWidget *parent = nullptr);

  ~MapView();

  QSize minimumSizeHint() const;
  QSize sizeHint() const;

  void attach(DefinitionManager *dm);
  void attach(QSharedPointer<ChunkCache> chunkCache_);

  void setLocation(double x, double z);
  void setLocation(double x, int y, double z, bool ignoreScale, bool useHeight);
  BlockLocation getLocation();
  BlockLocation getConfiguredLocation();
  void setDimension(QString path, int scale);
  void setFlags(int flags);
  int  getFlags() const;
  int  getDepth() const;
  void addOverlayItem(QSharedPointer<OverlayItem> item);
  void clearOverlayItems();
  void setVisibleOverlayItemTypes(const QSet<QString>& itemTypes);
  QList<QSharedPointer<OverlayItem>> getOverlayItems(const QString& type) const;

  // public for saving the png
  QString getWorldPath();

  void updatePlayerPositions(const QVector<PlayerInfo>& playerList);
  void updateSearchResultPositions(const QVector<QSharedPointer<OverlayItem> > &searchResults);

 public slots:
  void setDepth(int depth);
  void chunkUpdated(const QSharedPointer<Chunk>& chunk, int x, int z);
  void redraw();

  // Clears the cache and redraws, causing all chunks to be re-loaded;
  // but keeps the viewport
  void clearCache();

 signals:
  void hoverTextChanged(QString text);
  void demandDepthChange(int value);
  void demandDepthValue(int value);
  void showProperties(QVariant properties);
  void addOverlayItemType(QString type, QColor color);
  void coordinatesChanged(int x, int y, int z);
  void chunkRenderingCompleted(QSharedPointer<Chunk> chunk);

 protected:
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseDoubleClickEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void resizeEvent(QResizeEvent *event);
  void paintEvent(QPaintEvent *event);

 private slots:
  void addStructureFromChunk(QSharedPointer<GeneratedStructure> structure);

 private:

  using ChunkGroupIDListT = std::vector<std::pair<ChunkGroupID, QSharedPointer<RenderGroupData> > >;

  void redraw_drawMap(const ChunkGroupIDListT& cgidList, const QImage &placeholderImg, const MapCamera &camera, DrawHelper3& h2);

  void redraw_drawChunkCacheStatus(const ChunkGroupIDListT& cgidList, const QSharedPointer<RenderGroupData>& renderedDataDummy, QPainter &canvas, const MapCamera &camera);

  class AsyncRenderLock;

  RenderParams getCurrentRenderParams() const
  {
    RenderParams p;
    p.renderedAt = depth;
    p.renderedFlags = flags;
    return p;
  }

  template<class dataT>
  bool redrawNeeded(const dataT& renderedChunk) const
  {
    return (renderedChunk.renderedFor != getCurrentRenderParams());
  }

  void getToolTipMousePos(int mouse_x, int mouse_y);
  void getToolTip(int x, int z);
  void getToolTip_withChunkAvailable(int x, int z, const QSharedPointer<Chunk> &chunk);
  int getY(int x, int z);
  QList<QSharedPointer<OverlayItem>> getItems(int x, int y, int z);
  void adjustZoom(double steps);

  MapCamera getCamera() const;

  static const int CAVE_DEPTH = 16;  // maximum depth caves are searched in cave mode
  float caveshade[CAVE_DEPTH];

  QReadWriteLock m_readWriteLock;

  int depth;
  double x, z;
  int scale;

  double zoom;

  int flags;
  QTimer updateTimer;
  QSharedPointer<ChunkCache> cache;

  bool havePendingToolTip;
  ChunkID pendingToolTipChunk;
  QPoint pendingToolTipPos;

  using RenderedChunkGroupCacheUnprotectedT = SafeCache<ChunkGroupID, RenderGroupData>;
  using RenderedChunkGroupCacheT = LockGuarded<RenderedChunkGroupCacheUnprotectedT>;
  RenderedChunkGroupCacheT renderedChunkGroupsCache;

  QImage imageChunks;
  QImage imageOverlays;
  QImage image_players;
  DefinitionManager *dm;

  QSet<QString> overlayItemTypes;
  QMap<QString, QList<QSharedPointer<OverlayItem>>> overlayItems;
  BlockLocation currentLocation;

  QPoint lastMousePressPosition;
  bool dragging;

  QVector<QSharedPointer<OverlayItem> > currentPlayers;
  QVector<QSharedPointer<OverlayItem> > currentSearchResults;

  SafeGuiThreadInvoker m_invoker;
  QSharedPointer<PriorityThreadPool> threadpool_;
  SimpleSafePriorityThreadPoolWrapper safeThreadPoolI;

  SafeGuiThreadInvoker invoker;
  bool hasChanged;

  void changed();

  class AutoPerformance
  {
  public:
    AutoPerformance(std::chrono::milliseconds updateRateGoal_)
      : currentPerformance(1)
      , updateRateGoal(updateRateGoal_)
    {}

    size_t getCurrentPerformance() const
    {
      return currentPerformance;
    }

    void update(std::chrono::milliseconds actualUpdateRate)
    {
      if (actualUpdateRate < updateRateGoal)
      {
        if (currentPerformance < std::numeric_limits<size_t>::max())
        {
          currentPerformance++;
        }
      }
      else
      {
        currentPerformance /= 2;

        if (currentPerformance < 1)
        {
          currentPerformance = 1;
        }
      }
    }

  private:
    size_t currentPerformance;
    std::chrono::milliseconds updateRateGoal;
  };

  class AutoPerformanceTimer
  {
  public:
    AutoPerformanceTimer()
      : lastUpdate(std::chrono::steady_clock::now())
    {}

    std::chrono::steady_clock::duration updateTime()
    {
      const auto now = std::chrono::steady_clock::now();
      const auto duration = now - lastUpdate;
      lastUpdate = now;
      return duration;
    }

  private:
    std::chrono::steady_clock::time_point lastUpdate;
  };

  class AsyncLoop
  {
  public:
    AsyncLoop(const QSharedPointer<PriorityThreadPool>& threadpool,
              const JobPrio prio_,
              const std::function<void()>& func_);

  private:
    QSharedPointer<PriorityThreadPool> threadpool;
    const JobPrio prio;
    std::function<void()> func;
    SimpleSafePriorityThreadPoolWrapper safeThreadI;

    void requestNext();
  };

  class UpdateChecker
  {
  public:
    UpdateChecker(
        MapView& parent_,
        const QSharedPointer<ChunkCache>& cache,
        const QSharedPointer<PriorityThreadPool>& threadpool_,
        std::function<void(QSharedPointer<Chunk>)> renderChunkRequestFunction_);
    ~UpdateChecker();

    void update();

  private:
    MapView& parent;
    QSharedPointer<ChunkCache> cache;
    QSharedPointer<PriorityThreadPool> threadpool;
    std::function<void(QSharedPointer<Chunk>)> renderChunkRequestFunction;
    ThreadSafeQueue<ChunkID> chunksToRedraw;
    ChunkIteratorC chunkRedrawIterator;
    AutoPerformance autoPerformance;
    AutoPerformanceTimer autoPerformanceTimer;
    AsyncExecutionCancelGuard asyncGuard;
    std::mutex mutex;
    bool updateIsRunning;
    std::shared_ptr<std::function<void()> > idleJob;
    bool isIdleJobRegistered;

    void idleJobFunction();
    void registerIdleJobOfNotYetDone();
    void unregisterIdleJobIfNotJetDone();

    void regularUpdata__checkRedraw();
    void regularUpdata__checkRedraw_chunkGroup(const ChunkGroupID& cgid, RenderGroupData& data);

    void update_internal(bool regular);

    size_t getCurrentQueueLimit() const;
  };

  bool isRunning;
  std::shared_ptr<UpdateChecker> updateChecker;

  size_t renderChunkAsync(const QSharedPointer<Chunk> &chunk);

  void updateCacheSize(bool onlyIncrease);

  void changeEvent(QEvent *) override;

  void setBackgroundActivitiesEnabled(bool enabled);

private slots:
  void renderingDone(const QSharedPointer<RenderedChunk> chunk);

  void regularUpdate();
};

#endif  // MAPVIEW_H_
