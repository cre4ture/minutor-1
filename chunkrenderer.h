#ifndef CHUNKRENDERER_H
#define CHUNKRENDERER_H

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

#endif // CHUNKRENDERER_H
