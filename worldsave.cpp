/** Copyright (c) 2013, Sean Kasun */

/*
 Saves the world to PNG.  It doesn't use stock PNG code because
 the resulting image might be too large to fit into RAM.  Therefore,
 it uses a custom PNG generator that will handle *huge* worlds, but
 make less-than-optimal PNGs.
 */

#include "./worldsave.h"
#include "./mapview.h"
#include "./chunkrenderer.h"
#include "zlib/zlib.h"

#include "chunkrenderer.h"

WorldSave::WorldSave(QString filename, MapView *map,
                     bool regionChecker, bool chunkChecker,
                     int top, int left, int bottom, int right) :
  filename(filename),
  map(map),
  top(top),
  left(left),
  bottom(bottom),
  right(right),
  regionChecker(regionChecker),
  chunkChecker(chunkChecker) {
}

WorldSave::~WorldSave() {
}

static inline void w32(char *p, quint32 v) {
  *p++ = v >> 24;
  *p++ = (v >> 16) & 0xff;
  *p++ = (v >> 8) & 0xff;
  *p++ = v & 0xff;
}

static void writeChunk(QFile *f, const char *tag, const char *data,
                       int len) {
  char dword[4];
  w32(dword, len);
  f->write(dword, 4);
  f->write(tag, 4);
  if (len != 0)
    f->write(data, len);
  quint32 crc = crc32(0, Z_NULL, 0);
  crc = crc32(crc, (const Bytef *)tag, 4);
  if (len != 0)
    crc = crc32(crc, (const Bytef *)data, len);
  w32(dword, crc);
  f->write(dword, 4);
}

void WorldSave::run() {
  emit progress(tr("Calculating world bounds"), 0.0);
  QString path = map->getWorldPath();

  // convert from Blocks to Chunks
  top    = top/16;
  left   = left/16;
  bottom = bottom/16;
  right  = right/16;

  if ( top==0 && left==0 && right==0 && bottom==0)
    findBounds(path, &top, &left, &bottom, &right);

  int width  = (right + 1 - left) * 16;
  int height = (bottom + 1 - top) * 16;

  QFile png(filename);
  png.open(QIODevice::WriteOnly);

  // output PNG signature
  const char *sig = "\x89PNG\x0d\x0a\x1a\x0a";
  png.write(sig, 8);
  // output PNG header
  const char *ihdrdata = "\x00\x00\x00\x00"  // width
      "\x00\x00\x00\x00"  // height
      "\x08"  // bit depth
      "\x06"  // color type (rgba)
      "\x00"  // compresion method (deflate)
      "\x00"  // filter method (standard)
      "\x00";  // interlace method (none)
  char ihdr[13];
  memcpy(ihdr, ihdrdata, 13);
  w32(ihdr, width);
  w32(ihdr + 4, height);
  writeChunk(&png, "IHDR", ihdr, 13);

  int insize = width * 16 * 4 + 16;
  int outsize = insize * 2;

  uchar *scanlines = new uchar[insize];
  for (int i = 0; i < 16; i++)  // set scanline filters to off
    scanlines[i * (width * 4 + 1)] = 0;

  uchar *compressed = new uchar[outsize];
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  deflateInit2(&strm, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);

  double maximum = (bottom + 1 - top) * (right + 1 - left);
  double step = 0.0;
  for (int z = top; z <= bottom; z++) {
    for (int x = left; x <= right; x++, step += 1.0) {
      emit progress(tr("Rendering world"), step / maximum);
      int rx = x >> 5;
      int rz = z >> 5;
      QFile f(path + "/region/r." + QString::number(rx) + "." +
              QString::number(rz) + ".mca");
      if (!f.open(QIODevice::ReadOnly)) {
        blankChunk(scanlines, width * 4 + 1, x - left);
        continue;
      }
      uchar *header = f.map(0, 4096);
      int offset = 4 * ((x & 31) + (z & 31) * 32);
      int coffset = (header[offset] << 16) | (header[offset + 1] << 8) |
          header[offset + 2];
      int numSectors = header[offset + 3];
      f.unmap(header);
      if (coffset == 0) {
        // no chunk here
        blankChunk(scanlines, width * 4 + 1, x - left);
      } else {
        uchar *raw = f.map(coffset * 4096, numSectors * 4096);
        NBT nbt(raw);
        QSharedPointer<Chunk> chunk(new Chunk());
        chunk->load(nbt);
        f.unmap(raw);
        drawChunk(scanlines, width * 4 + 1, x - left, chunk);
        chunk.reset();
      }
      f.close();
    }
    // write out scanlines to disk
    strm.avail_in = insize;
    strm.next_in = scanlines;
    do {
      strm.avail_out = outsize;
      strm.next_out = compressed;
      deflate(&strm, (z == bottom) ? Z_FINISH : Z_NO_FLUSH);
      writeChunk(&png, "IDAT", (const char *)compressed,
                 outsize - strm.avail_out);
    } while (strm.avail_out == 0);
  }
  deflateEnd(&strm);
  delete [] scanlines;
  delete [] compressed;

  writeChunk(&png, "IEND", NULL, 0);
  png.close();
  emit finished();
}


typedef struct {
  int x, z;
} ChunkPos;

// helper functions for the findBounds function

static bool outside(int side, const ChunkPos &edge, const ChunkPos &p) {
  switch (side) {
    case 0:  // top
      return edge.z > p.z;
    case 1:  // left
      return edge.x > p.x;
    case 2:  // bottom
      return edge.z < p.z;
    default:  // right
      return edge.x < p.x;
  }
}
static bool onside(int side, const ChunkPos &edge, const ChunkPos &p) {
  switch (side) {
    case 0:  // top or bottom
    case 2:
      return edge.z == p.z;
    default:  // left or right
      return edge.x == p.x;
  }
}


/*
   This routine loops through all the region filenames and constructs a list
   of regions that lie along the edges of the map.  Then it loops through
   those region headers and finds the furthest chunks.  The end result is
   the boundary chunks for the entire world.

   Because we only check at the chunk level, there could be up to 15 pixels
   of padding around the edge of the final image.  However, if we just
   went by regions, there could be 511 pixels of padding.
   */
void WorldSave::findBounds(QString path, int *top, int *left, int *bottom,
                           int *right) {
  QStringList filters;
  filters << "*.mca";

  QDirIterator it(path + "/region", filters);
  QList<ChunkPos> edges[4];
  ChunkPos cur;
  bool hasOne = false;

  // loop through all region files and find the extremes
  while (it.hasNext()) {
    it.next();
    QString fn = it.fileName();
    int len = fn.length() - 4;  // length of filename

    // figure out the X of the region
    int posX = 2;  // position after "r."
    int posE = posX;
    while (posE < len && (fn.at(posE) == '+' || fn.at(posE) == '-' ||
                          fn.at(posE).isDigit())) {
      posE++;
    }
    QStringRef numX(&fn, posX, posE-posX);
    cur.x = numX.toInt();

    // figure out the Z of the region
    int posZ = ++posE;
    while (posE < len && (fn.at(posE) == '+' || fn.at(posE) == '-' ||
                          fn.at(posE).isDigit())) {
      posE++;
    }
    QStringRef numZ(&fn, posZ, posE - posZ);
    cur.z = numZ.toInt();

    if (!hasOne) {
      for (int e = 0; e < 4; e++)
        edges[e].append(cur);
      hasOne = true;
    }
    for (int e = 0; e < 4; e++)
      if (outside(e, edges[e].front(), cur)) {
        edges[e].clear();
        edges[e].append(cur);
      } else if (onside(e, edges[e].front(), cur)) {
        edges[e].append(cur);
      }
  }
  // find image bounds
  int minz = 32, maxz = 0, minx = 32, maxx = 0;
  for (int e = 0; e < 4; e++) {
    for (int i = 0; i < edges[e].length(); i++) {
      QFile f(path+"/region/r." +
              QString::number(edges[e].at(i).x) + "." +
              QString::number(edges[e].at(i).z) + ".mca");
      f.open(QIODevice::ReadOnly);
      uchar *header = f.map(0, 4096);
      // loop through all chunk headers.
      for (int offset = 0; offset < 4096; offset += 4) {
        int coffset = (header[offset] << 16) | (header[offset + 1] << 8) |
            header[offset + 2];
        if (coffset != 0) {
          switch (e) {
            case 0:  // smallest Z
              minz = qMin(minz, offset / 128);
              break;
            case 1:  // smallest X
              minx = qMin(minx, (offset & 127) / 4);
              break;
            case 2:  // largest Z
              maxz = qMax(maxz, offset / 128);
              break;
            case 3:  // largest X
              maxx = qMax(maxx, (offset & 127) / 4);
              break;
          }
        }
      }
      f.unmap(header);
      f.close();
    }
  }
  *top = (edges[0].front().z * 32) + minz;
  *left = (edges[1].front().x * 32) + minx;
  *bottom = (edges[2].front().z * 32) + maxz;
  *right = (edges[3].front().x * 32) + maxx;
}

// sets chunk to transparent
void WorldSave::blankChunk(uchar *scanlines, int stride, int x) {
  int offset = x * 16 * 4 + 1;
  for (int y = 0; y < 16; y++, offset += stride)
    memset(scanlines + offset, 0, 16 * 4);
}

void WorldSave::drawChunk(uchar *scanlines, int stride, int x, QSharedPointer<Chunk> chunk) {
  // calculate attenuation
  float attenuation = 1.0f;
  if (this->regionChecker && static_cast<int>(floor(chunk->chunkX / 32.0f) +
                                              floor(chunk->chunkZ / 32.0f)) % 2 != 0)
    attenuation *= 0.9f;
  if (this->chunkChecker && ((chunk->chunkX + chunk->chunkZ) % 2) != 0)
    attenuation *= 0.9f;

  // render chunk with current settings
  ChunkRenderer::renderChunk(*map, chunk.get());
  // we can't memcpy each scanline because it's in BGRA format.
  int offset = x * 16 * 4 + 1;
  int ioffset = 0;
  for (int y = 0; y < 16; y++, offset += stride) {
    int xofs = offset;
    for (int x = 0; x < 16; x++, xofs += 4) {
      scanlines[xofs+2] = attenuation * chunk->image[ioffset++];
      scanlines[xofs+1] = attenuation * chunk->image[ioffset++];
      scanlines[xofs+0] = attenuation * chunk->image[ioffset++];
      scanlines[xofs+3] = attenuation * chunk->image[ioffset++];
    }
  }
}
