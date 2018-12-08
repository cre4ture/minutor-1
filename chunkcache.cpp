/** Copyright (c) 2013, Sean Kasun */

#include "./chunkcache.h"
#include "./chunkloader.h"

#include <QMetaType>

#if defined(__unix__) || defined(__unix) || defined(unix)
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

ChunkID::ChunkID()
    : x(0)
    , z(0)
{}

ChunkID::ChunkID(int x, int z) : x(x), z(z) {
}
bool ChunkID::operator==(const ChunkID &other) const {
    return (other.x == x) && (other.z == z);
}

bool ChunkID::operator<(const ChunkID &other) const
{
    return (other.x < x) || ((other.x == x) && (other.z < z));
}
uint qHash(const ChunkID &c) {
  return (c.x << 16) ^ (c.z & 0xffff);  // safe way to hash a pair of integers
}

ChunkCache::ChunkCache()
    : mutex(QMutex::Recursive)
{
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
  //cache.setMaxCost(chunks);
  maxcache = 2 * chunks;  // most chunks are less than half filled with sections

  qRegisterMetaType<QSharedPointer<Chunk> >("QSharedPointer<Chunk>");
  qRegisterMetaType<ChunkID>("ChunkID");
}

ChunkCache::~ChunkCache() {
}

void ChunkCache::clear() {
  QThreadPool::globalInstance()->waitForDone();
  mutex.lock();
  state.clear();
  cache.clear();
  mutex.unlock();
}

void ChunkCache::setPath(QString path) {
  this->path = path;
}
QString ChunkCache::getPath() {
  return path;
}

QSharedPointer<Chunk> ChunkCache::fetch(int x, int z, FetchBehaviour behav)
{
  ChunkID id(x, z);

  {
      QMutexLocker locker(&mutex);
      auto chunkState = state[id];
      if ((behav == FetchBehaviour::USE_CACHED) && (chunkState == ChunkState::Cached))
      {
          return cache[id];
      }
      else
      {
          loadChunkAsync(id);
          return nullptr;
      }
  }
}

bool ChunkCache::isLoaded(int x, int z,  QSharedPointer<Chunk> &chunkPtr_out)
{
    ChunkID id(x, z);

    {
        QMutexLocker locker(&mutex);
        chunkPtr_out = cache[id];
        return (chunkPtr_out != nullptr);
    }
}

void ChunkCache::gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id)
{
    cache[id] = chunk;
    state[id] = ChunkState::Cached;

    emit chunkLoaded(chunk != nullptr, id.getX(), id.getZ());
}

void ChunkCache::loadChunkAsync(ChunkID id)
{
    {
        QMutexLocker locker(&mutex);

        auto latestState = state[id];
        if (latestState == ChunkState::Loading)
        {
            return; // prevent loading chunk twice
        }

        cache[id] = nullptr;
        state[id] = ChunkState::Loading;
    }

    ChunkLoader *loader = new ChunkLoader(path, id);
    connect(loader, SIGNAL(chunkUpdated(QSharedPointer<Chunk>, ChunkID)),
            this, SLOT(gotChunk(const QSharedPointer<Chunk>&, ChunkID)));
    QThreadPool::globalInstance()->start(loader);
}

void ChunkCache::adaptCacheToWindow(int x, int y) {
  int chunks = ((x + 15) >> 4) * ((y + 15) >> 4);  // number of chunks visible
  chunks *= 1.10;  // add 10%
  //cache.setMaxCost(qMin(chunks, maxcache));
}
