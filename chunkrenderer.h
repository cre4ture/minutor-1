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

   signals:
    void chunkRenderingCompleted(QSharedPointer<Chunk> chunk);

   protected:

    void run() override;

   private:
    QSharedPointer<Chunk> m_chunk;
    MapView& parent;

    void renderChunk(Chunk *chunk);
};

#endif // CHUNKRENDERER_H
