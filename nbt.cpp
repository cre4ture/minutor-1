/** Copyright (c) 2013, Sean Kasun */
#include <QFile>
#include <QByteArray>
#include <QDebug>
#include <QStringList>

#include "./nbt.h"
#include "zlib/zlib.h"

static void unzip_nbt(QByteArray& data, QByteArray& nbt)
{
    z_stream strm;
    static const int CHUNK_SIZE = 8192;
    char out[CHUNK_SIZE];
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = data.size();
    strm.next_in = reinterpret_cast<Bytef *>(data.data());

    inflateInit2(&strm, 15 + 32);
    do {
      strm.avail_out = CHUNK_SIZE;
      strm.next_out = reinterpret_cast<Bytef *>(out);
      inflate(&strm, Z_NO_FLUSH);
      nbt.append(out, CHUNK_SIZE - strm.avail_out);
    } while (strm.avail_out == 0);
    inflateEnd(&strm);
}

// this handles decoding the gzipped level.dat
NBT::NBT(const QString level) {
  root = &NBT::Null;  // just in case we die

  QFile f(level);
  f.open(QIODevice::ReadOnly);
  QByteArray data = f.readAll();
  f.close();

  QByteArray nbt;
  unzip_nbt(data, nbt);

  TagDataStream s(nbt.constData(), nbt.size());

  if (s.r8() == 10) {  // compound
    s.skip(s.r16());  // skip name
    root = new Tag_Compound(&s);
  }
}

// this handles decoding a compressed() section of a region file
NBT::NBT(const uchar *chunk) {
  root = &NBT::Null;  // just in case

  // find chunk size
  int length = (chunk[0] << 24) | (chunk[1] << 16) | (chunk[2] << 8) |
      chunk[3];
  if (chunk[4] != 2)  // rfc1950
    return;

  z_stream strm;
  static const int CHUNK_SIZE = 8192;
  char out[CHUNK_SIZE];
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = length - 1;
  strm.next_in = (Bytef *)chunk + 5;

  QByteArray nbt;

  inflateInit(&strm);
  do {
    strm.avail_out = CHUNK_SIZE;
    strm.next_out = reinterpret_cast<Bytef *>(out);
    inflate(&strm, Z_NO_FLUSH);
    nbt.append(out, CHUNK_SIZE - strm.avail_out);
  } while (strm.avail_out == 0);
  inflateEnd(&strm);

  TagDataStream s(nbt.constData(), nbt.size());

  if (s.r8() == 10) {  // compound
    s.skip(s.r16());  // skip name
    root = new Tag_Compound(&s);
  }
}

Tag NBT::Null;

bool NBT::has(const QString key) const {
  return root->has(key);
}

const Tag *NBT::at(const QString key) const {
  return root->at(key);
}

NBT::~NBT() {
  if (root != &NBT::Null)
    delete root;
}

/********** TAGS ****************/

Tag::Tag() {
}
Tag::~Tag() {
}
int Tag::length() const {
  qWarning() << "Unhandled length";
  return 0;
}
bool Tag::has(const QString) const {
  return false;
}
const Tag *Tag::at(const QString) const {
  return &NBT::Null;
}
const Tag *Tag::at(int /* idx */) const {
  return &NBT::Null;
}
const QString Tag::toString() const {
  qWarning() << "Unhandled toString";
  return "";
}
qint32 Tag::toInt() const {
  qWarning() << "Unhandled toInt";
  return 0;
}
double Tag::toDouble() const {
  qWarning() << "Unhandled toDouble";
  return 0.0;
}
const quint8 *Tag::toByteArray() const {
  qWarning() << "Unhandled toByteArray";
  return NULL;
}
const qint32 *Tag::toIntArray() const {
  qWarning() << "Unhandled toIntArray";
  return NULL;
}
const qint64 *Tag::toLongArray() const {
  qWarning() << "Unhandled toLongArray";
  return NULL;
}
const QVariant Tag::getData() const {
  qWarning() << "Unhandled getData";
  return QVariant();
}

// Tag_Byte

Tag_Byte::Tag_Byte(TagDataStream *s)
    : TagBigEndian_t(s)
{}

int Tag_Byte::toInt() const {
  return (signed char)data;
}

unsigned int Tag_Byte::toUInt() const {
  return (unsigned char)data;
}

// Tag_Short

Tag_Short::Tag_Short(TagDataStream *s)
    : TagBigEndian_t(s)
{}

int Tag_Short::toInt() const {
  return (signed short)data;
}

unsigned int Tag_Short::toUInt() const {
  return (unsigned short)data;
}

// Tag_Int

Tag_Int::Tag_Int(TagDataStream *s)
    : TagBigEndian_t(s)
{}

qint32 Tag_Int::toInt() const {
  return data;
}

double Tag_Int::toDouble() const {
  return static_cast<double>(data);
}

// Tag_Long

Tag_Long::Tag_Long(TagDataStream *s)
    : TagBigEndian_t(s)
{}


double Tag_Long::toDouble() const {
  return static_cast<double>(data);
}

qint32 Tag_Long::toInt() const {
  return static_cast<qint32>(data);
}

// Tag_Float

Tag_Float::Tag_Float(TagDataStream *s)
    : TagBigEndian_t(s)
{}

double Tag_Float::toDouble() const {
  return data;
}

// Tag_Double

Tag_Double::Tag_Double(TagDataStream *s)
    : TagBigEndian_t(s)
{}

double Tag_Double::toDouble() const {
  return data;
}

// Tag_Byte_Array

Tag_Byte_Array::Tag_Byte_Array(TagDataStream *s)
    : Tag_BigEndianArray_t(s)
{}

Tag_Byte_Array::~Tag_Byte_Array() {}

const quint8 *Tag_Byte_Array::toByteArray() const {
  return &data_array[0];
}

const QVariant Tag_Byte_Array::getData() const {
  return QByteArray(reinterpret_cast<const char*>(&data_array[0]), data_array.size());
}

const QString Tag_Byte_Array::toString() const {
  try {
    return QString::fromLatin1(reinterpret_cast<const char *>(&data_array[0]));
  } catch(...) {}

  return "<Binary data>";
}

// Tag_String

Tag_String::Tag_String(TagDataStream *s) {
  int len = s->r16();
  data = s->utf8(len);
}
const QString Tag_String::toString() const {
  return data;
}

// Tag_List

template <class T>
static void setListData(QList<Tag *> *data, int len,
                        TagDataStream *s) {
  for (int i = 0; i < len; i++)
    data->append(new T(s));
}

Tag_List::Tag_List(TagDataStream *s)
{
  quint8 type = s->r8();
  int len = s->r32();

    init(len, type, s);
}

Tag_List::Tag_List(int len, int type, TagDataStream *s)
{
    init(len, type, s);
}

void Tag_List::init(const int len, const int type, TagDataStream *s)
{
    if (len == 0)  // empty list, type is invalid
      return;

  switch (type) {
    case 1: setListData<Tag_Byte>(&data, len, s); break;
    case 2: setListData<Tag_Short>(&data, len, s); break;
    case 3: setListData<Tag_Int>(&data, len, s); break;
    case 4: setListData<Tag_Long>(&data, len, s); break;
    case 5: setListData<Tag_Float>(&data, len, s); break;
    case 6: setListData<Tag_Double>(&data, len, s); break;
    case 7: setListData<Tag_Byte_Array>(&data, len, s); break;
    case 8: setListData<Tag_String>(&data, len, s); break;
    case 9: setListData<Tag_List>(&data, len, s); break;
    case 10: setListData<Tag_Compound>(&data, len, s); break;
    case 11: setListData<Tag_Int_Array>(&data, len, s); break;
    case 12: setListData<Tag_Long_Array>(&data, len, s); break;
    default: throw "Unknown type";
  }
}

Tag_List::~Tag_List() {
  for (auto i = data.constBegin(); i != data.constEnd(); i++)
    delete *i;
}
int Tag_List::length() const {
  return data.count();
}
const Tag *Tag_List::at(int index) const {
  return data[index];
}

const QString Tag_List::toString() const {
  QStringList ret;
  ret << "[";
  for (auto i = data.constBegin(); i != data.constEnd(); i++) {
    ret << (*i)->toString();
    ret << ", ";
  }
  ret.last() = "]";
  return ret.join("");
}

const QVariant Tag_List::getData() const {
  QList<QVariant> lst;
  for (auto i = data.constBegin(); i != data.constEnd(); i++) {
    lst << (*i)->getData();
  }
  return lst;
}

// Tag_Compound

Tag_Compound::Tag_Compound(TagDataStream *s) {
  quint8 type;
  while ((type = s->r8()) != 0) {
    // until tag_end
    quint16 len = s->r16();
    QString key = s->utf8(len);
    Tag *child;
    switch (type) {
      case 1: child = new Tag_Byte(s); break;
      case 2: child = new Tag_Short(s); break;
      case 3: child = new Tag_Int(s); break;
      case 4: child = new Tag_Long(s); break;
      case 5: child = new Tag_Float(s); break;
      case 6: child = new Tag_Double(s); break;
      case 7: child = new Tag_Byte_Array(s); break;
      case 8: child = new Tag_String(s); break;
      case 9: child = new Tag_List(s); break;
      case 10: child = new Tag_Compound(s); break;
      case 11: child = new Tag_Int_Array(s); break;
      case 12: child = new Tag_Long_Array(s); break;
      default: throw "Unknown tag";
    }
    children[key] = child;
  }
}
Tag_Compound::~Tag_Compound() {
  for (auto i = children.constBegin(); i != children.constEnd(); i++)
    delete i.value();
}
bool Tag_Compound::has(const QString key) const {
  return children.contains(key);
}
const Tag *Tag_Compound::at(const QString key) const {
  if (!children.contains(key))
    return &NBT::Null;
  return children[key];
}

const QString Tag_Compound::toString() const {
  QStringList ret;
  ret << "{\n";
  for (auto i = children.constBegin(); i != children.constEnd(); i++) {
    ret << "\t" << i.key() << " = '" << i.value()->toString() << "',\n";
  }
  ret.last() = "}";
  return ret.join("");
}

const QVariant Tag_Compound::getData() const {
  QMap<QString, QVariant> map;
  for (auto i = children.constBegin(); i != children.constEnd(); i++) {
    map.insert(i.key(), i.value()->getData());
  }
  return map;
}

// Tag_Int_Array

Tag_Int_Array::Tag_Int_Array(TagDataStream *s)
    : Tag_BigEndianArray_t(s)
{}

const qint32 *Tag_Int_Array::toIntArray() const {
    return &data_array[0];
}

// Tag_Long_Array

Tag_Long_Array::Tag_Long_Array(TagDataStream *s)
    : Tag_BigEndianArray_t(s)
{}

const qint64 *Tag_Long_Array::toLongArray() const {
  return &data_array[0];
}

// TagDataStream

TagDataStream::TagDataStream(const char *data, int len) {
  this->data = (const quint8 *)data;
  this->len = len;
  pos = 0;
}

quint8 *TagDataStream::r(size_t len) {
  // you need to free anything read with this
  quint8 *r = new quint8[len];
  rBuffer(r, len);
  return r;
}

void TagDataStream::rBuffer(quint8 *buffer, size_t len)
{
    memcpy(buffer, data + pos, len);
    pos += len;
}

QString TagDataStream::utf8(int len) {
  int old = pos;
  pos += len;
  return QString::fromUtf8((const char *)data + old, len);
}
void TagDataStream::skip(int len) {
  pos += len;
}
