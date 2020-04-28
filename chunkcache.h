/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKCACHE_H_
#define CHUNKCACHE_H_

#include "./chunk.h"
#include "./coordinateid.h"
#include "safecache.hpp"
#include "enumbitset.hpp"
#include "chunkloader.h"
#include "coordinatehashmap.h"
#include "jobprio.h"

#include <QObject>
#include <QCache>
#include <QSharedPointer>
#include "./chunkid.h"
enum class CacheState {
  uncached,
  uncached_loading,
  cached // still can be nullptr when empty
};

class ChunkCache : public QObject {
  Q_OBJECT

public:
  ChunkCache(const QSharedPointer<PriorityThreadPool>& threadPool);
  ~ChunkCache();

  static ChunkCache& Instance();

private:
  static ChunkCache* globalInstance;

  ChunkCache(const ChunkCache &) = delete;
  ChunkCache &operator=(const ChunkCache &) = delete;

 public:
  void clear();
  void setPath(QString path);
  QString getPath() const;
  QSharedPointer<Chunk> fetch(int cx, int cz);         // fetch Chunk and load when not found
  CacheState getCached(const ChunkID& id, QSharedPointer<Chunk>& chunk_out);    // fetch Chunk only if cached, can tell if just not loaded or empty
  QSharedPointer<Chunk> getChunkSynchronously(const ChunkID& id);         // get chunk if cached directly, or load it in a synchronous blocking way
  int getCacheUsage() const;
  int getCacheMax() const;
  int getMemoryMax() const;

  class Locker
  {
  public:
      Locker(ChunkCache& parent)
          : m_parent(parent)
          , m_locker(&parent.mutex)
      {}

      bool isCached(ChunkID id, QSharedPointer<Chunk> &chunkPtr_out)
      {
          return m_parent.isCached_unprotected(id, &chunkPtr_out);
      }

      bool isCached(ChunkID id)
      {
          return m_parent.isCached_unprotected(id, nullptr);
      }

      bool fetch(QSharedPointer<Chunk>& chunk_out,
                 ChunkID id,
                 JobPrio priority = JobPrio::low)
      {
          return m_parent.fetch_unprotected(chunk_out, id, priority);
      }

      void triggerReload(ChunkID id)
      {
        return m_parent.loadChunkAsync_unprotected(id, JobPrio::low);
      }

  private:
      ChunkCache& m_parent;
      QMutexLocker m_locker;
  };

 signals:
  void chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z);
  void structureFound(QSharedPointer<GeneratedStructure> structure);

 public slots:
  void setCacheMaxSize(int chunks);

 private slots:
  void routeStructure(QSharedPointer<GeneratedStructure> structure);
  void gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id, const ExecutionGuard &guard);

 private:
  QString path;                                   // path to folder with region files

  enum class ChunkState {
    Loading,
    NonExisting,
  };

  using ChunkInfoT = Bitset<ChunkState, uint8_t>;

  SafeCache<ChunkID, Chunk> cache;           // real Cache
  CoordinateHashMap<ChunkInfoT> chunkStates;
  QMutex mutex;                                   // Mutex for accessing the Cache
  int maxcache;                                   // number of Chunks that fit into memory
  QThreadPool loaderThreadPool;                   // extra thread pool for loading

  QSharedPointer<PriorityThreadPool> threadPool__;
  SimpleSafePriorityThreadPoolWrapper safeThreadPoolI; // last member!

  void loadChunkAsync_unprotected(ChunkID id,
                                  JobPrio priority);

  bool isCached_unprotected(ChunkID id, QSharedPointer<Chunk>* chunkPtr_out);

  bool fetch_unprotected(QSharedPointer<Chunk> &chunk_out, ChunkID id, JobPrio priority);

};

#endif  // CHUNKCACHE_H_
