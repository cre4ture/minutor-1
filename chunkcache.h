/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKCACHE_H_
#define CHUNKCACHE_H_

#include "./chunk.h"
#include "./chunkcachetypes.h"
#include "safecache.hpp"
#include "enumbitset.hpp"

#include <QObject>
#include <QCache>
#include <QSharedPointer>

class ChunkLoaderThreadPool;


class ChunkCache : public QObject {
  Q_OBJECT

 public:
  // singleton: access to global usable instance
  static QSharedPointer<ChunkCache> Instance();
 private:
  // singleton: prevent access to constructor and copyconstructor
  ChunkCache();
  ~ChunkCache();
  ChunkCache(const ChunkCache &);
  ChunkCache &operator=(const ChunkCache &);

 public:
  void clear();
  void setPath(QString path);
  QString getPath() const;
  QSharedPointer<Chunk> fetch(int cx, int cz);         // fetch Chunk and load when not found
  int getCost() const;
  int getMaxCost() const;

  enum class FetchBehaviour
  {
      USE_CACHED,
      USE_CACHED_OR_UDPATE,
      FORCE_UPDATE
  };

  class Locker
  {
  public:
      Locker(ChunkCache& parent)
          : m_parent(parent)
          , m_locker(&parent.mutex)
      {}

      bool isCached(ChunkID id, QSharedPointer<Chunk> &chunkPtr_out)
      {
          return m_parent.isCached_unprotected(id, chunkPtr_out);
      }

      bool fetch(QSharedPointer<Chunk>& chunk_out, ChunkID id, FetchBehaviour behav = FetchBehaviour::USE_CACHED_OR_UDPATE)
      {
          return m_parent.fetch_unprotected(chunk_out, id, behav);
      }

  private:
      ChunkCache& m_parent;
      QMutexLocker m_locker;
  };

 signals:
  void chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z);
  void structureFound(QSharedPointer<GeneratedStructure> structure);

 public slots:
  void adaptCacheToWindow(int wx, int wy);

 private slots:
  void routeStructure(QSharedPointer<GeneratedStructure> structure);
  void gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id);

 private:
  QString path;                                   // path to folder with region files

  enum class ChunkState {
      Loading,
      NonExisting,
  };

  using ChunkInfoT = Bitset<ChunkState, uint8_t>;

  SafeCache<ChunkID, Chunk> cache;           // real Cache
  QHash<ChunkID, ChunkInfoT> chunkStates;
  QMutex mutex;                                   // Mutex for accessing the Cache
  int maxcache;                                   // number of Chunks that fit into Cache
  QThreadPool loaderThreadPool;                   // extra thread pool for loading

  QSharedPointer<ChunkLoaderThreadPool> m_loaderPool;

  void loadChunkAsync_unprotected(ChunkID id);

  bool isCached_unprotected(ChunkID id, QSharedPointer<Chunk> &chunkPtr_out);

  bool fetch_unprotected(QSharedPointer<Chunk> &chunk_out, ChunkID id, FetchBehaviour behav = FetchBehaviour::USE_CACHED_OR_UDPATE);

};

#endif  // CHUNKCACHE_H_
