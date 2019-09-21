/** Copyright (c) 2013, Sean Kasun */
#ifndef MAPVIEW_H_
#define MAPVIEW_H_

#include "./chunkcache.h"
#include "./playerinfos.h"

#include <QtWidgets/QWidget>
#include <QSharedPointer>

#include <QVector>
#include <set>

class DefinitionManager;
class BiomeIdentifier;
class BlockIdentifier;
class OverlayItem;
class DrawHelper;
class DrawHelper2;
class ChunkRenderer;
class AsyncTaskProcessorBase;

class MapView : public QWidget {
  Q_OBJECT

    friend class DrawHelper;
    friend class ChunkRenderer;
    friend class DrawHelper2;

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
  void drawChunk(int x, int z, DrawHelper2 &h, ChunkCache::Locker &locked_cache);
  void drawChunk2(int x, int z, const QSharedPointer<Chunk> &chunk, DrawHelper2 &h);
  void drawChunk3(int x, int z, const QSharedPointer<RenderedChunk> &chunk, DrawHelper2 &h);
  void getToolTipMousePos(int mouse_x, int mouse_y);
  void getToolTip(int x, int z);
  int getY(int x, int z);
  QList<QSharedPointer<OverlayItem>> getItems(int x, int y, int z);

  struct TopViewPosition
  {
      TopViewPosition(double _x, double _z)
          : x(_x)
          , z(_z)
      {}

      double x;
      double z;
  };

  TopViewPosition transformMousePos(QPoint mouse_pos);

  static const int CAVE_DEPTH = 16;  // maximum depth caves are searched in cave mode
  float caveshade[CAVE_DEPTH];

  QReadWriteLock m_readWriteLock;

  int depth;
  double x, z;
  int scale;

  double zoom_internal;

  double getZoom() const;
  void adjustZoom(double rate);

  int flags;
  QTimer updateTimer;
  QSharedPointer<ChunkCache> cache;
  QImage imageChunks;
  QImage imageOverlays;
  QImage image_players;
  std::set<ChunkID> chunksToLoad;
  std::set<ChunkID> chunksToRedraw;
  DefinitionManager *dm;
  uchar placeholder[16 * 16 * 4];  // no chunk found placeholder
  QSet<QString> overlayItemTypes;
  QMap<QString, QList<QSharedPointer<OverlayItem>>> overlayItems;
  BlockLocation currentLocation;

  QPoint lastMousePressPosition;
  bool dragging;

  QVector<QSharedPointer<OverlayItem> > currentPlayers;
  QVector<QSharedPointer<OverlayItem> > currentSearchResults;

  enum ChunkRenderState
  {
      NONE,
      RENDERING,
  };

  QMutex m_renderStatesMutex;
  QMap<ChunkID, ChunkRenderState> renderStates;

  QSharedPointer<AsyncTaskProcessorBase> m_asyncRendererPool;

  class AsyncRenderLock
  {
  public:
      AsyncRenderLock(MapView& parent_)
          : m_locker(&parent_.m_renderStatesMutex)
          , m_parent(parent_)
      {}

      void renderChunkAsync(const QSharedPointer<Chunk> &chunk);

  private:
      QMutexLocker m_locker;
      MapView& m_parent;
  };

  void emit_chunkRenderingCompleted(const QSharedPointer<Chunk>& chunk);

private slots:
    void renderingDone(const QSharedPointer<Chunk>& chunk);

    void regularUpdate();
};

#endif  // MAPVIEW_H_
