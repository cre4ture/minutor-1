/** Copyright (c) 2013, Sean Kasun */
#ifndef NBT_H_
#define NBT_H_

class QString;
class QByteArray;

#include <QHash>
#include <QString>
#include <QVariant>

#include <vector>

class TagDataStream {
 public:
  TagDataStream(const char *data, int len);
  quint8 r8() { return data[pos++]; }
  quint16 r16() { return rBigEndian_t<quint16>(); }
  quint32 r32() { return rBigEndian_t<quint32>(); }
  quint64 r64() { return rBigEndian_t<quint64>(); }
  quint8 *r(size_t len);
  void rBuffer(quint8* buffer, size_t len);
  QString utf8(int len);

  template<typename _ValueT>
  _ValueT rBigEndian_t();

  template<typename _ValueT>
  void rBigEndianArray_t(_ValueT* outarray, const size_t len);

  template<int _size>
  void rBigEndian_x(quint8* buffer);

  template<int _size>
  void rBigEndianArray_x(quint8* outarray, const size_t len);


  void skip(int len);
 private:
  const quint8 *data;
  int pos;
  const int len;
};

template<typename _ValueT>
inline void TagDataStream::rBigEndianArray_t(_ValueT *data_array, const size_t len)
{
    rBigEndianArray_x<sizeof(_ValueT)>(reinterpret_cast<quint8*>(data_array), len);
}

template<int _size>
inline void TagDataStream::rBigEndian_x(quint8 *buffer)
{
    for (size_t i = 0; i < _size; i++)
    {
        buffer[_size-i-1] = r8();
    }
}

template<>
inline void TagDataStream::rBigEndianArray_x<1>(quint8 *data_array, const size_t len)
{
    rBuffer(data_array, len);
}

template<int _size>
inline void TagDataStream::rBigEndianArray_x(quint8 *data_array, const size_t len)
{
    const size_t end = len*_size;
    for (size_t i = 0; i < end; i += _size)
        rBigEndian_x<_size>(&data_array[i]);
}

template<typename _ValueT>
inline _ValueT TagDataStream::rBigEndian_t()
{
    _ValueT value;
    rBigEndian_x<sizeof(_ValueT)>(reinterpret_cast<quint8*>(&value));
    return value;
}

class Tag {
 public:
  Tag();
  virtual ~Tag();
  virtual bool has(const QString key) const;
  virtual int length() const;
  virtual const Tag *at(const QString key) const;
  virtual const Tag *at(int index) const;
  virtual const QString toString() const;
  virtual qint32 toInt() const;
  virtual double toDouble() const;
  virtual const quint8 *toByteArray() const;
  virtual const qint32 *toIntArray() const;
  virtual const qint64 *toLongArray() const;
  virtual const QVariant getData() const;
};

template<typename _ValueT>
class TagWithData_t: public Tag
{
public:
    virtual const QVariant getData() const override
    {
        return data;
    }

protected:
    _ValueT data;
};

template<typename _ValueT>
class TagBigEndian_t: public TagWithData_t<_ValueT>
{
public:
    TagBigEndian_t(TagDataStream *s)
    {
        data = s->rBigEndian_t<_ValueT>();
    }

    virtual const QString toString() const override
    {
        return QString::number(data);
    }

protected:
    _ValueT data;
};

class NBT {
 public:
  explicit NBT(const QString level);
  explicit NBT(const uchar *chunk);
  ~NBT();

  bool has(const QString key) const;
  const Tag *at(const QString key) const;

  static Tag Null;
 private:
  Tag *root;
};

class Tag_Byte : public TagBigEndian_t<quint8> {
 public:
  explicit Tag_Byte(TagDataStream *s);
  int toInt() const;
  unsigned int toUInt() const;
};

class Tag_Short : public TagBigEndian_t<qint16> {
 public:
  explicit Tag_Short(TagDataStream *s);
  int toInt() const;
  unsigned int toUInt() const;
};

class Tag_Int : public TagBigEndian_t<qint32> {
 public:
  explicit Tag_Int(TagDataStream *s);
  qint32 toInt() const;
  double toDouble() const;
};

class Tag_Long : public TagBigEndian_t<qint64> {
 public:
  explicit Tag_Long(TagDataStream *s);
  qint32 toInt() const;
  double toDouble() const;
};

class Tag_Float : public TagBigEndian_t<float> {
 public:
  explicit Tag_Float(TagDataStream *s);

  virtual double toDouble() const;
};

class Tag_Double : public TagBigEndian_t<double> {
 public:
  explicit Tag_Double(TagDataStream *s);
  double toDouble() const;
};

template<typename _ValueT>
class Tag_BigEndianArray_t : public Tag {
public:
    explicit Tag_BigEndianArray_t(TagDataStream *s)
        : data_array()
    {
        const quint32 len = s->r32();
        data_array.resize(len);
        s->rBigEndianArray_t<_ValueT>(&data_array[0], len);
    }

    ~Tag_BigEndianArray_t() {}

    int length() const override { return data_array.size(); }

    virtual const QString toString() const override
    {
        QStringList ret;
        ret << "[";
        for (size_t i = 0; i < data_array.size(); ++i) {
          ret << QString::number(data_array[i]) << ",";
        }
        ret.last() = "]";
        return ret.join("");
    }

    virtual const QVariant getData() const override
    {
        QList<QVariant> ret;
        for (size_t i = 0; i < data_array.size(); ++i) {
          ret.push_back(data_array[i]);
        }

        return ret;
    }

protected:
    std::vector<_ValueT> data_array;
};


class Tag_Byte_Array : public Tag_BigEndianArray_t<quint8> {
public:
    explicit Tag_Byte_Array(TagDataStream *s);
    ~Tag_Byte_Array();

    const quint8 *toByteArray() const override;
    virtual const QString toString() const override;
    virtual const QVariant getData() const override;
};

class Tag_String : public TagWithData_t<QString> {
 public:
  explicit Tag_String(TagDataStream *s);
  const QString toString() const;
};

class Tag_List : public Tag {
 public:
  explicit Tag_List(TagDataStream *s);
  explicit Tag_List(int len, int type, TagDataStream *s);
  ~Tag_List();
  const Tag *at(int index) const;
  int length() const;
  virtual const QString toString() const;
  virtual const QVariant getData() const;
 private:
  QList<Tag *> data;

  void init(const int len, const int type, TagDataStream *s);
};

class Tag_Compound : public Tag {
 public:
  explicit Tag_Compound(TagDataStream *s);
  ~Tag_Compound();
  bool has(const QString key) const;
  const Tag *at(const QString key) const;
  virtual const QString toString() const;
  virtual const QVariant getData() const;
 private:
  QHash<QString, Tag *> children;
};

class Tag_Int_Array : public Tag_BigEndianArray_t<qint32> {
 public:
  explicit Tag_Int_Array(TagDataStream *s);
  const qint32 *toIntArray() const;
};

class Tag_Long_Array : public Tag_BigEndianArray_t<qint64> {
 public:
  explicit Tag_Long_Array(TagDataStream *s);
  const qint64 *toLongArray() const;
};

#endif  // NBT_H_
