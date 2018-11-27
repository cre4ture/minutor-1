/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKLOADER_H_
#define CHUNKLOADER_H_

#include "chunkcachetypes.h"

#include <QObject>
#include <QRunnable>

class Chunk;
class ChunkID;
class QMutex;

class ChunkLoader : public QObject, public QRunnable {
  Q_OBJECT

 public:
  ChunkLoader(QString path, ChunkID id_);
  ~ChunkLoader();
 signals:
  void loaded(int x, int z);
  void chunkUpdated(QSharedPointer<Chunk> chunk, ChunkID id);
 protected:
  void run();
  QSharedPointer<Chunk> runInternal();
 private:
  QString path;
  ChunkID id;
};

#endif  // CHUNKLOADER_H_
