/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKLOADER_H_
#define CHUNKLOADER_H_

#include "chunkcachetypes.h"
#include "threadsafequeue.hpp"

#include <QObject>
#include <QRunnable>

class Chunk;
class ChunkID;
class QMutex;

class ChunkLoaderThreadPool : public QObject
{
    Q_OBJECT

public:
    ChunkLoaderThreadPool();

    typedef std::pair<QString, ChunkID> JobT;

    void enqueueChunkLoading(QString path, ChunkID id)
    {
        m_queue.push(JobT(path, id));
    }

signals:
    void chunkUpdated(QSharedPointer<Chunk> chunk, ChunkID id);

private:
    class ImplC;

    QSharedPointer<ImplC> m_impl;
    ThreadSafeQueue<JobT>& m_queue;

    void signalUpdated(QSharedPointer<Chunk> chunk, ChunkID id);
};

class ChunkLoader
{
 public:
  ChunkLoader(QString path, ChunkID id_);
  ~ChunkLoader();

  void run();

  QSharedPointer<Chunk> runInternal();
 private:
  QString path;
  ChunkID id;
};

#endif  // CHUNKLOADER_H_
