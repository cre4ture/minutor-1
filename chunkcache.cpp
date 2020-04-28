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

ChunkCache* ChunkCache::globalInstance = nullptr;

ChunkCache &ChunkCache::Instance()
{
  if (!globalInstance)
  {
    throw std::runtime_error("ChunkCache::Instance(): no instance set!");
  }
  return *globalInstance;
}

ChunkCache::ChunkCache(const QSharedPointer<PriorityThreadPool>& threadPool_)
    : cache("chunks")
    , chunkStates()
    , mutex(QMutex::Recursive)
    , threadPool__(threadPool_)
    , safeThreadPoolI(*threadPool__)
{
  globalInstance = this;

  chunkStates.reserve(256*1024*1024);

  const int sizeChunkMax     = sizeof(Chunk) + 16 * sizeof(ChunkSection);  // all sections contain Blocks
  const int sizeChunkTypical = sizeof(Chunk) + 6 * sizeof(ChunkSection);   // world generation is average Y=64..128

  // default: 10% more than 1920x1200 blocks
  int chunks = 10000;
  maxcache = chunks;

  // try to determine available pysical memory based on operation system we are running on
#if defined(__unix__) || defined(__unix) || defined(unix)
#ifdef _SC_AVPHYS_PAGES
  auto pages = sysconf(_SC_AVPHYS_PAGES);
  auto page_size = sysconf(_SC_PAGE_SIZE);
  quint64 available = (pages*page_size);
  chunks   = available / sizeChunkMax;
  maxcache = available / sizeChunkTypical;  // most chunks are less filled with sections
#endif
#elif defined(_WIN32) || defined(WIN32)
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  DWORDLONG available = qMin(status.ullAvailPhys, status.ullAvailVirtual);
  chunks   = available / sizeChunkMax;
  maxcache = available / sizeChunkTypical;  // most chunks are less filled with sections
#endif
  // we start the Cache based on worst case calculation
  cache.setMaxCost(chunks);

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
  globalInstance = nullptr;
}

void ChunkCache::clear() {

  std::unique_ptr<AsyncExecutionCancelGuard> oldGuard;

  {
    QMutexLocker guard(&mutex);
    oldGuard = safeThreadPoolI.renewCancellationAndReturnOld();
    oldGuard->startCancellation(); // start cancellation of everything started up to now, before locking mutex.

    cache.clear();
    chunkStates.clear();
  }

  oldGuard.reset(); // waits for cancellation done (this line is just making it more clear)
}

void ChunkCache::setPath(QString path) {
  if (this->path != path)
    clear();
  this->path = path;
}
QString ChunkCache::getPath() const {
  return path;
}

int ChunkCache::getCacheUsage() const {
  return cache.totalCost();
}

int ChunkCache::getCacheMax() const {
  return cache.maxCost();
}

int ChunkCache::getMemoryMax() const {
  return maxcache;
}

bool ChunkCache::fetch_unprotected(QSharedPointer<Chunk>& chunk_out,
                                   ChunkID id,
                                   JobPrio priority)
{
  const bool cached = isCached_unprotected(id, &chunk_out);

  if (!cached)
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

QSharedPointer<Chunk> ChunkCache::getChunkSynchronously(const ChunkID& id)
{
  ExecutionGuard guard;

  {
    QMutexLocker locker(&mutex);

    QSharedPointer<Chunk> chunk;
    if (isCached_unprotected(id, &chunk))
    {
      return chunk;
    }

    auto& chunkState = chunkStates[id];
    chunkState << ChunkState::Loading;

    guard = safeThreadPoolI.getCancelToken().createExecutionGuardChecked();
  }

  ChunkLoader loader(path, id);
  auto chunk = loader.runInternal();

  gotChunk(chunk, id, guard);

  return chunk;
}

void ChunkCache::gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id, const ExecutionGuard& guard)
{
  {
    QMutexLocker locker(&mutex);

    guard.checkCancellation();

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

  safeThreadPoolI.enqueueJob([this, id](const ExecutionGuard& guard){

    ChunkLoader loader(path, id);
    auto chunk = loader.runInternal();

    guard.checkCancellation();

    gotChunk(chunk, id, guard);

  }, priority);
}

void ChunkCache::setCacheMaxSize(int chunks) {
  QMutexLocker guard(&mutex);
  // we never decrease Cache size, and never exceed physical memory
  cache.setMaxCost(std::max<int>(cache.maxCost(), std::min<int>(chunks, maxcache)));
}

