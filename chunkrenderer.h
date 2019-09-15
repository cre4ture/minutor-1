/** Copyright (c) 2019, EtlamGit */
#ifndef CHUNKRENDERER_H
#define CHUNKRENDERER_H

#include <QObject>
#include <QRunnable>
#include "chunkcache.h"

#include <QObject>
#include <QRunnable>
#include <QSharedPointer>

class MapView;
class Chunk;

class ChunkRenderer: public QObject, public QRunnable {
    Q_OBJECT

   public:
    ChunkRenderer(const QSharedPointer<Chunk> &chunk, MapView& parent_);

    ~ChunkRenderer() override;

    static void renderChunk(MapView& parent, Chunk *chunk);

   signals:
    void chunkRenderingCompleted(QSharedPointer<Chunk> chunk);

   protected:

    void run() override;

   private:
    QSharedPointer<Chunk> m_chunk;
    MapView& m_parent;

};

class CaveShade {
 public:
  // singleton: access to global usable instance
  static float getShade(int index);
 private:
  // singleton: prevent access to constructor and copyconstructor
  CaveShade();
  ~CaveShade() {}
  CaveShade(const CaveShade &);
  CaveShade &operator=(const CaveShade &);

 public:
  static const int CAVE_DEPTH = 16;  // maximum depth caves are searched in cave mode
  float caveshade[CAVE_DEPTH];
};

#endif // CHUNKRENDERER_H
