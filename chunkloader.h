/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKLOADER_H_
#define CHUNKLOADER_H_

#include "chunkcachetypes.h"
#include "threadsafequeue.hpp"
#include "asynctaskprocessorbase.hpp"

#include <QObject>
#include <QRunnable>

class Chunk;
class ChunkID;
class QMutex;
class NBT;

class ChunkLoaderThreadPool : public QObject, public AsyncTaskProcessorBase
{
  Q_OBJECT

public:
    void enqueueChunkLoading(QString path, ChunkID id);

signals:
    void chunkUpdated(QSharedPointer<Chunk> chunk, ChunkID id);

private:
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
