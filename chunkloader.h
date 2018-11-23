/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKLOADER_H_
#define CHUNKLOADER_H_

#include <QObject>
#include <QRunnable>
class Chunk;
class ChunkID;
class QMutex;

class ChunkLoader : public QObject, public QRunnable {
  Q_OBJECT

 public:
  ChunkLoader(QString path, int x, int z, const QMap<ChunkID, Chunk*> &cache,
              QMutex *mutex);
  ~ChunkLoader();
 signals:
  void loaded(int x, int z);
  void chunkUpdated(bool success, int x, int z);
 protected:
  void run();
  bool runInternal();
 private:
  QString path;
  int x, z;
  const QMap<ChunkID, Chunk*> &cache;
  QMutex *mutex;
};

#endif  // CHUNKLOADER_H_
