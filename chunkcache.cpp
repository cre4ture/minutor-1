/** Copyright (c) 2013, Sean Kasun */

#include "./chunkcache.h"
#include "./chunkloader.h"
#include "prioritythreadpool.h"

#include <QMetaType>
#include <iostream>

#if defined(__unix__) || defined(__unix) || defined(unix)
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

ChunkCache::ChunkCache(const QSharedPointer<PriorityThreadPool>& threadPool_)
    : cache("chunks")
    , chunkStates()
    , mutex(QMutex::Recursive)
    , threadPool(threadPool_)
    , loaderPool(QSharedPointer<ChunkLoaderThreadPool>::create(threadPool))
    , asyncGuard(*this)
{
  chunkStates.reserve(256*1024*1024);

  int chunks = 10000;  // 10% more than 1920x1200 blocks
#if defined(__unix__) || defined(__unix) || defined(unix)
#ifdef _SC_AVPHYS_PAGES
  auto pages = sysconf(_SC_AVPHYS_PAGES);
  auto page_size = sysconf(_SC_PAGE_SIZE);
  chunks = (pages*page_size) / (sizeof(Chunk) + 16*sizeof(ChunkSection));
#endif
#elif defined(_WIN32) || defined(WIN32)
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  DWORDLONG available = qMin(status.ullAvailPhys, status.ullAvailVirtual);
  chunks = available / (sizeof(Chunk) + 16 * sizeof(ChunkSection));
#endif
  cache.setMaxCost(chunks);
  maxcache = 2 * chunks;  // most chunks are less than half filled with sections

  //cache.setMaxCost(maxcache);

  // determain optimal thread pool size for "loading"
  // as this contains disk access, use less than number of cores
  int tmax = loaderThreadPool.maxThreadCount();
  loaderThreadPool.setMaxThreadCount(tmax / 2);

  qRegisterMetaType<QSharedPointer<GeneratedStructure>>("QSharedPointer<GeneratedStructure>");

  qRegisterMetaType<QSharedPointer<Chunk> >("QSharedPointer<Chunk>");
  qRegisterMetaType<ChunkID>("ChunkID");

  clear();
}

ChunkCache::~ChunkCache() {
  loaderThreadPool.waitForDone();
}

void ChunkCache::clear() {

  QMutexLocker guard(&mutex);

  loaderPool.reset(); // stop everything!

  cache.clear();
  chunkStates.clear();

  loaderPool = QSharedPointer<ChunkLoaderThreadPool>::create(threadPool); // start again
  connect(loaderPool.data(), SIGNAL(chunkUpdated(QSharedPointer<Chunk>, ChunkID)),
          this, SLOT(gotChunk(const QSharedPointer<Chunk>&, ChunkID)));
}

void ChunkCache::setPath(QString path) {
  if (this->path != path)
    clear();
  this->path = path;
}
QString ChunkCache::getPath() const {
  return path;
}

int ChunkCache::getCost() const {
  return cache.totalCost();
}

int ChunkCache::getMaxCost() const {
  return cache.maxCost();
}

QSharedPointer<Chunk> ChunkCache::getChunkSynchronously(ChunkID id)
{
  {
    QMutexLocker locker(&mutex);

    QSharedPointer<Chunk> chunk;
    if (isCached_unprotected(id, &chunk))
    {
      return chunk;
    }

    auto& chunkState = chunkStates[id];
    chunkState << ChunkState::Loading;
  }

  ChunkLoader loader(path, id);
  auto chunk = loader.runInternal();

  gotChunk(chunk, id);

  return chunk;
}

bool ChunkCache::fetch_unprotected(QSharedPointer<Chunk>& chunk_out,
                                   ChunkID id,
                                   FetchBehaviour behav,
                                   JobPrio priority)
{
  const bool cached = isCached_unprotected(id, &chunk_out);

  if ( (behav == FetchBehaviour::FORCE_UPDATE) ||
       (
           (behav == FetchBehaviour::USE_CACHED_OR_UDPATE) && (!cached)
       )
    )
  {
      loadChunkAsync_unprotected(id, priority);
      chunk_out.reset();
      return false;
  }
  return cached;
}

bool ChunkCache::isCached_unprotected(ChunkID id, QSharedPointer<Chunk>* chunkPtr_out)
{
  auto& chunkState = chunkStates[id];
  if (chunkState.test(ChunkState::NonExisting))
  {
    if (chunkPtr_out)
    {
      (*chunkPtr_out).reset();
    }
    return true; // state not existing is known -> cached that there is no chunk!
  }
  else
  {
    if (cache.contains(id))
    {
      if (chunkPtr_out)
      {
        *chunkPtr_out = cache[id];
      }

      return true;
    }
    else
    {
      return false;
    }
  }
}

void ChunkCache::routeStructure(QSharedPointer<GeneratedStructure> structure) {
  emit structureFound(structure);
}

void ChunkCache::gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id)
{
  {
    QMutexLocker locker(&mutex);

    auto& chunkState = chunkStates[id];
    chunkState.unset(ChunkState::Loading);

    if (!chunk)
    {
      chunkState.set(ChunkState::NonExisting);
      emit chunkLoaded(QSharedPointer<Chunk>(), id.getX(), id.getZ()); // signal that chunk information about non existend chunk is available now
      return;
    }

    cache.insert(id, chunk);

  }

  if (chunk)
  {
    for (const auto& structure: chunk->structurelist)
    {
      emit structureFound(structure);
    }
  }

  emit chunkLoaded(chunk, id.getX(), id.getZ());

  //std::cout << "cached chunks: " << cachemap.size() << std::endl;
}

void ChunkCache::loadChunkAsync_unprotected(ChunkID id,
                                            JobPrio priority)
{
    {
      auto& chunkState = chunkStates[id];

      if (chunkState[ChunkState::Loading])
      {
          return; // prevent loading chunk twice
      }

      chunkState << ChunkState::Loading;
    }

  threadPool->enqueueJob([id, cancelToken = asyncGuard.getWeakAccessor()](){

    auto guard = cancelToken.safeAccess();

    ChunkLoader loader(guard.first.path, id);
    auto chunk = loader.runInternal();

    guard.first.gotChunk(chunk, id);

  }, priority);
}

void ChunkCache::adaptCacheToWindow(int wx, int wy) {
  int chunks = ((wx + 15) >> 4) * ((wy + 15) >> 4);  // number of chunks visible
  chunks *= 1.10;  // add 10%

  QMutexLocker guard(&mutex);
  cache.setMaxCost(qMin(chunks, maxcache));
}
