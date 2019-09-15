/** Copyright (c) 2013, Sean Kasun */

#include "./chunkcache.h"
#include "./chunkloader.h"

#include <QMetaType>
#include <iostream>

#if defined(__unix__) || defined(__unix) || defined(unix)
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

ChunkID::ChunkID()
    : cx(0)
    , cz(0)
{}

ChunkID::ChunkID(int cx, int cz) : cx(cx), cz(cz) {
}
bool ChunkID::operator==(const ChunkID &other) const {
    return (other.cx == cx) && (other.cz == cz);
}

bool ChunkID::operator<(const ChunkID &other) const
{
    return (other.cx < x) || ((other.cx == cx) && (other.z < z));
}
uint qHash(const ChunkID &c) {
  return (c.cx << 16) ^ (c.cz & 0xffff);  // safe way to hash a pair of integers
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

  // determain optimal thread pool size for "loading"
  // as this contains disk access, use less than number of cores
  int tmax = loaderThreadPool.maxThreadCount();
  loaderThreadPool.setMaxThreadCount(tmax / 2);

  qRegisterMetaType<QSharedPointer<GeneratedStructure>>("QSharedPointer<GeneratedStructure>");

  qRegisterMetaType<QSharedPointer<Chunk> >("QSharedPointer<Chunk>");
  qRegisterMetaType<ChunkID>("ChunkID");

  connect(m_loaderPool.get(), SIGNAL(chunkUpdated(QSharedPointer<Chunk>, ChunkID)),
          this, SLOT(gotChunk(const QSharedPointer<Chunk>&, ChunkID)));
}

ChunkCache::~ChunkCache() {
  loaderThreadPool.waitForDone();
}

ChunkCache& ChunkCache::Instance() {
  static ChunkCache singleton;
  return singleton;
}

void ChunkCache::clear() {
  QThreadPool::globalInstance()->waitForDone();
  mutex.lock();
  cachemap.clear();
  mutex.unlock();
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

int ChunkCache::getMaxCost() const {
  return cache.maxCost();
}

bool ChunkCache::fetch_unprotected(QSharedPointer<Chunk>& chunk_out, ChunkID id, FetchBehaviour behav)
{
  const auto& chunkInfo = cachemap[id];
  const bool cached = chunkInfo.state[ChunkState::Cached];

  if ( (behav == FetchBehaviour::FORCE_UPDATE) ||
       (
           (behav == FetchBehaviour::USE_CACHED_OR_UDPATE) && (!cached)
       )
    )
  {
      loadChunkAsync_unprotected(id);
      chunk_out = nullptr;
      return false;
  }

  chunk_out = chunkInfo.chunk;
  return cached;
}

bool ChunkCache::isLoaded_unprotected(ChunkID id,  QSharedPointer<Chunk> &chunkPtr_out)
{
    chunkPtr_out = cachemap[id].chunk;
    return (chunkPtr_out != nullptr);
}

bool ChunkCache::isCached_unprotected(ChunkID id, QSharedPointer<Chunk> &chunkPtr_out)
{
    ChunkInfoT& info = cachemap[id];
    chunkPtr_out = info.chunk;
    return info.state.test(ChunkState::Cached);
}

QSharedPointer<Chunk> ChunkCache::fetchCached(int cx, int cz) {
  // try to get Chunk from Cache
  ChunkID id(cx, cz);
  mutex.lock();
  QSharedPointer<Chunk> * p_chunk(cache[id]);   // const operation
  mutex.unlock();

  if (p_chunk != NULL )
    return QSharedPointer<Chunk>(*p_chunk);
  else
    return QSharedPointer<Chunk>(NULL);  // we're loading this chunk, or it's blank.
}

QSharedPointer<Chunk> ChunkCache::fetch(int cx, int cz) {
  // try to get Chunk from Cache
  ChunkID id(cx, cz);
  mutex.lock();
  QSharedPointer<Chunk> * p_chunk(cache[id]);   // const operation
  mutex.unlock();
  if (p_chunk != NULL ) {
    QSharedPointer<Chunk> chunk(*p_chunk);
    if (chunk->loaded)
      return chunk;
    return QSharedPointer<Chunk>(NULL);  // we're loading this chunk, or it's blank.
  }
  // launch background process to load this chunk
  p_chunk = new QSharedPointer<Chunk>(new Chunk());
  connect(p_chunk->data(), SIGNAL(structureFound(QSharedPointer<GeneratedStructure>)),
          this,            SLOT  (routeStructure(QSharedPointer<GeneratedStructure>)));
  mutex.lock();
  cache.insert(id, p_chunk);    // non-const operation !
  mutex.unlock();
  ChunkLoader *loader = new ChunkLoader(path, cx, cz);
  connect(loader, SIGNAL(loaded(int, int)),
          this,   SLOT(gotChunk(int, int)));
  loaderThreadPool.start(loader);
  return QSharedPointer<Chunk>(NULL);
}

void ChunkCache::gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id)
{
    QMutexLocker locker(&mutex);

    auto& chunkInfo = cachemap[id];
    chunkInfo.chunk = chunk;
    chunkInfo.state << ChunkState::Cached;
    chunkInfo.state.unset(ChunkState::Loading);

    emit chunkLoaded(chunk, id.getX(), id.getZ());

    //std::cout << "cached chunks: " << cachemap.size() << std::endl;
}

void ChunkCache::routeStructure(QSharedPointer<GeneratedStructure> structure) {
  emit structureFound(structure);
}

void ChunkCache::loadChunkAsync_unprotected(ChunkID id)
{
    {
        auto& chunkInfo = cachemap[id];

        if (chunkInfo.state[ChunkState::Loading])
        {
            return; // prevent loading chunk twice
        }

        chunkInfo.state << ChunkState::Loading;
    }

    m_loaderPool->enqueueChunkLoading(path, id);
}

void ChunkCache::adaptCacheToWindow(int wx, int wy) {
  int chunks = ((wx + 15) >> 4) * ((wy + 15) >> 4);  // number of chunks visible
  chunks *= 1.10;  // add 10%
  cache.setMaxCost(qMin(chunks, maxcache));
}
