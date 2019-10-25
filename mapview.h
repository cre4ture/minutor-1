/** Copyright (c) 2013, Sean Kasun */
#ifndef MAPVIEW_H_
#define MAPVIEW_H_

#include "./chunkcache.h"
#include "./playerinfos.h"
#include "mapcamera.hpp"

#include "lockguarded.hpp"
#include "enumbitset.hpp"

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
class AsyncTaskProcessorBase;

MapCamera CreateCameraForChunkGroup(const ChunkGroupID& cgid);

class MapView : public QWidget {
  Q_OBJECT

    friend class DrawHelper;
    friend class ChunkRenderer;
    friend class DrawHelper2;
    friend class DrawHelper3;

 public:
  /// Values for the individual flags
  enum {
    flgLighting     = 1,
    flgMobSpawn     = 2,
    flgCaveMode     = 4,
    flgDepthShading = 8,
    flgShowEntities = 16,
    flgSingleLayer = 32,
    flgBiomeColors  = 64
  };

  typedef struct {
    float x, y, z;
    int scale;

    QVector3D getPos3D() const
    {
        return QVector3D(x,y,z);
    }

  } BlockLocation;

  explicit MapView(const QSharedPointer<AsyncTaskProcessorBase>& threadpool, QWidget *parent = nullptr);

  QSize minimumSizeHint() const;
  QSize sizeHint() const;

  void attach(DefinitionManager *dm);
  void attach(QSharedPointer<ChunkCache> chunkCache_);

  void setLocation(double x, double z);
  void setLocation(double x, int y, double z, bool ignoreScale, bool useHeight);
  BlockLocation getLocation();
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

  MapCamera getCamera() const;

  static const int CAVE_DEPTH = 16;  // maximum depth caves are searched in cave mode
  float caveshade[CAVE_DEPTH];

  QReadWriteLock m_readWriteLock;

  int depth;
  double x, z;
  int scale;

  double zoom;

  void adjustZoom(double steps);

  int flags;
  QTimer updateTimer;
  QSharedPointer<ChunkCache> cache;

  bool havePendingToolTip;
  ChunkID pendingToolTipChunk;
  QPoint pendingToolTipPos;

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

  using RenderedChunkGroupCacheUnprotectedT = SafeCache<ChunkGroupID, RenderGroupData>;
  using RenderedChunkGroupCacheT = LockGuarded<RenderedChunkGroupCacheUnprotectedT>;
  RenderedChunkGroupCacheT renderedChunkGroupsCache;

  QImage imageChunks;
  QImage imageOverlays;
  QImage image_players;
  QQueue<ChunkID> chunksToLoad;
  QQueue<std::pair<ChunkID, QSharedPointer<Chunk>>> chunksToRedraw;
  DefinitionManager *dm;

  static const QImage &getPlaceholder();
  static const QImage &getChunkGroupPlaceholder();

  QSet<QString> overlayItemTypes;
  QMap<QString, QList<QSharedPointer<OverlayItem>>> overlayItems;
  BlockLocation currentLocation;

  QPoint lastMousePressPosition;
  bool dragging;

  QVector<QSharedPointer<OverlayItem> > currentPlayers;
  QVector<QSharedPointer<OverlayItem> > currentSearchResults;

  QSharedPointer<AsyncTaskProcessorBase> m_asyncRendererPool;

  ChunkIteratorC chunkRedrawIterator;

  size_t renderChunkAsync(const QSharedPointer<Chunk> &chunk);

  void updateCacheSize(bool onlyIncrease);

private slots:
    void renderingDone(const QSharedPointer<RenderedChunk> chunk);

    void regularUpdate();
    void regularUpdata__checkRedraw();
    void regularUpdata__checkRedraw_chunkGroup(const ChunkGroupID& cgid, RenderGroupData& data);
};

#endif  // MAPVIEW_H_
