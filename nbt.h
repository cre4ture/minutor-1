/** Copyright (c) 2013, Sean Kasun */
#ifndef NBT_H_
#define NBT_H_

class QString;
class QByteArray;

#include <QHash>
#include <QString>
#include <QVariant>

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

  template<int _size>
  void rBigEndian_x(quint8* buffer);

  void skip(int len);
 private:
  const quint8 *data;
  int pos, len;
};

template<int _size>
inline void TagDataStream::rBigEndian_x(quint8 *buffer)
{
    for (size_t i = 0; i < _size; i++)
    {
        buffer[_size-i-1] = r8();
    }
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
  virtual const QVariant getData() const;
};

template<typename _ValueT>
class TagBigEndian_t: public Tag
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

    virtual const QVariant getData() const override
    {
        return data;
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
};

class Tag_Short : public TagBigEndian_t<qint16> {
 public:
  explicit Tag_Short(TagDataStream *s);
  int toInt() const;
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

class Tag_Byte_Array : public Tag {
 public:
  explicit Tag_Byte_Array(TagDataStream *s);
  ~Tag_Byte_Array();
  int length() const;
  const quint8 *toByteArray() const;
  virtual const QString toString() const;
  virtual const QVariant getData() const;
 private:
  const quint8 *data;
  int len;
};

class Tag_String : public Tag {
 public:
  explicit Tag_String(TagDataStream *s);
  const QString toString() const;
  virtual const QVariant getData() const;
 private:
  QString data;
};

class Tag_List : public Tag {
 public:
  explicit Tag_List(TagDataStream *s);
  ~Tag_List();
  const Tag *at(int index) const;
  int length() const;
  virtual const QString toString() const;
  virtual const QVariant getData() const;
 private:
  QList<Tag *> data;
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

class Tag_Int_Array : public Tag {
 public:
  explicit Tag_Int_Array(TagDataStream *s);
  ~Tag_Int_Array();
  int length() const;
  const qint32 *toIntArray() const;
  virtual const QString toString() const;
  virtual const QVariant getData() const;
 private:
  int len;
  qint32 *data;
};

#endif  // NBT_H_
