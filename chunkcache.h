/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKCACHE_H_
#define CHUNKCACHE_H_

#include "./chunk.h"
#include "./chunkcachetypes.h"

#include <QObject>
#include <QCache>
#include <QSharedPointer>


class ChunkCache : public QObject {
  Q_OBJECT

 public:
  ChunkCache();
  ~ChunkCache();
  void clear();
  void setPath(QString path);
  QString getPath();

  enum class FetchBehaviour
  {
      USE_CACHED,
      FORCE_UPDATE
  };

  QSharedPointer<Chunk> fetch(int x, int z, FetchBehaviour behav = FetchBehaviour::USE_CACHED);

  bool isLoaded(int x, int z, QSharedPointer<Chunk> &chunkPtr_out);

 signals:
  void chunkLoaded(bool success, int x, int z);

 public slots:
  void adaptCacheToWindow(int x, int y);

 private slots:
  void gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id);

 private:
  QString path;

  enum class ChunkState {
      Uncached,
      Loading,
      Cached
  };

  QMap<ChunkID, ChunkState> state;
  ChunkCacheT cache;
  QMutex mutex;
  int maxcache;

  void loadChunkAsync(ChunkID id);
};

#endif  // CHUNKCACHE_H_
