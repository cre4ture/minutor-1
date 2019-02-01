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
    , m_loaderPool(QSharedPointer<ChunkLoaderThreadPool>::create())
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

  connect(m_loaderPool.get(), SIGNAL(chunkUpdated(QSharedPointer<Chunk>, ChunkID)),
          this, SLOT(gotChunk(const QSharedPointer<Chunk>&, ChunkID)));
}

ChunkCache::~ChunkCache() {
}

void ChunkCache::clear() {
  QThreadPool::globalInstance()->waitForDone();
  mutex.lock();
  cachemap.clear();
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
      const auto& chunkInfo = cachemap[id];
      if ((behav == FetchBehaviour::USE_CACHED) && (chunkInfo.state[ChunkState::Cached]))
      {
          return chunkInfo.chunk;
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
        chunkPtr_out = cachemap[id].chunk;
        return (chunkPtr_out != nullptr);
    }
}

void ChunkCache::gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id)
{
    QMutexLocker locker(&mutex);

    auto& chunkInfo = cachemap[id];
    chunkInfo.chunk = chunk;
    chunkInfo.state << ChunkState::Cached;
    chunkInfo.state.unset(ChunkState::Loading);

    emit chunkLoaded(chunk != nullptr, id.getX(), id.getZ());
}

void ChunkCache::loadChunkAsync(ChunkID id)
{
    {
        QMutexLocker locker(&mutex);

        auto& chunkInfo = cachemap[id];

        if (chunkInfo.state[ChunkState::Loading])
        {
            return; // prevent loading chunk twice
        }

        chunkInfo.state << ChunkState::Loading;
    }

    m_loaderPool->enqueueChunkLoading(path, id);
}

void ChunkCache::adaptCacheToWindow(int x, int y) {
  int chunks = ((x + 15) >> 4) * ((y + 15) >> 4);  // number of chunks visible
  chunks *= 1.10;  // add 10%
  //cache.setMaxCost(qMin(chunks, maxcache));
}
