/** Copyright (c) 2013, Sean Kasun */

#include "./chunkloader.h"
#include "./chunkcache.h"
#include "./chunk.h"

#include <future>

ChunkLoader::ChunkLoader(QString path, ChunkID id_)
    : path(path)
    , id(id_)
{}

ChunkLoader::~ChunkLoader() {
}

void ChunkLoader::run() {

    auto newChunk = runInternal();
    if (newChunk)
    {
        //emit loaded(id.getX(), id.getZ());
    }

    //emit chunkUpdated(newChunk, id);
}

QSharedPointer<Chunk> ChunkLoader::runInternal()
{
    //thread()->msleep(300);

    int rx = id.getX() >> 5;
    int rz = id.getZ() >> 5;

    QFile f(path + "/region/r." + QString::number(rx) + "." +
            QString::number(rz) + ".mca");
    if (!f.open(QIODevice::ReadOnly)) {  // no chunks in this region
      return nullptr;
    }
    // map header into memory
    uchar *header = f.map(0, 4096);
    int offset = 4 * ((id.getX() & 31) + (id.getZ() & 31) * 32);
    int coffset = (header[offset] << 16) | (header[offset + 1] << 8) |
        header[offset + 2];
    int numSectors = header[offset+3];
    f.unmap(header);

    if (coffset == 0) {  // no chunk
      f.close();
      return nullptr;
    }

    uchar *raw = f.map(coffset * 4096, numSectors * 4096);
    if (raw == nullptr) {
      f.close();
      return nullptr;
    }
    NBT nbt(raw);

    auto chunk = QSharedPointer<Chunk>::create();
    chunk->load(nbt);

    f.unmap(raw);
    f.close();

    return chunk;
}

class ChunkLoaderThreadPool::ImplC
{
public:
    ImplC(ChunkLoaderThreadPool& parent)
        : m_parent(parent)
    {
        for (size_t i = 0; i < 8; i++)
        {
             m_futures.push_back(std::async(std::launch::async, [this]() {
                ChunkLoaderThreadPool::JobT job;
                while (m_queue.pop(job))
                {
                    ChunkLoader loader(job.first, job.second);
                    auto chunk = loader.runInternal();
                    m_parent.signalUpdated(chunk, job.second);
                }
            }));
        }
    }

    ~ImplC()
    {
        m_queue.signalTerminate();
        for (auto& future: m_futures)
        {
            future.get();
        }
    }

    ChunkLoaderThreadPool& m_parent;
    ThreadSafeQueue<JobT> m_queue;
    std::list<std::future<void> > m_futures;
};

ChunkLoaderThreadPool::ChunkLoaderThreadPool()
    : m_impl(QSharedPointer<ImplC>::create(*this))
    , m_queue(m_impl->m_queue)
{

}

void ChunkLoaderThreadPool::signalUpdated(QSharedPointer<Chunk> chunk, ChunkID id)
{
    emit chunkUpdated(chunk, id);
}
