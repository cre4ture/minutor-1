/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKLOADER_H_
#define CHUNKLOADER_H_

#include "coordinateid.h"
#include "cancellation.h"
#include "jobprio.h"
#include "safeprioritythreadpoolwrapper.h"

#include <QObject>
#include <QRunnable>

class Chunk;
class ChunkID;
class QMutex;
class NBT;
class PriorityThreadPool;

class ChunkLoaderThreadPool : public QObject
{
  Q_OBJECT

public:
  ChunkLoaderThreadPool(const QSharedPointer<PriorityThreadPool>& threadPool);
  ~ChunkLoaderThreadPool();

  void enqueueChunkLoading(QString path,
                           ChunkID id,
                           JobPrio priority);

signals:
  void chunkUpdated(QSharedPointer<Chunk> chunk, ChunkID id);

private:
  QSharedPointer<PriorityThreadPool> threadPool_;
  SimpleSafePriorityThreadPoolWrapper safeThreadPoolI;      // must be last member


  void signalUpdated(QSharedPointer<Chunk> chunk, ChunkID id);
};

class ChunkLoader
{
 public:
  ChunkLoader(QString path, ChunkID id_);
  ~ChunkLoader();

  void run();

  QSharedPointer<NBT> loadNbt();

  QSharedPointer<Chunk> runInternal();

  static QString getRegionFilename(const QString& path, const ChunkID& id);

 private:
  QString path;
  ChunkID id;
};

#endif  // CHUNKLOADER_H_
