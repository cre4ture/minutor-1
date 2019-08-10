/** Copyright (c) 2013, Sean Kasun */
#ifndef JSON_H_
#define JSON_H_

#include <QHash>
#include <QList>

class JSONHelper;

class JSONData {
 public:
  JSONData();
  virtual ~JSONData();
  virtual bool has(const QString key);
  virtual JSONData *at(const QString key);
  virtual JSONData *at(int index);
  virtual int length();
  virtual QString asString();
  virtual double asNumber();
  virtual bool asBool();

  template<typename _ValueT>
  _ValueT asValue();

  template<typename _ValueT>
  _ValueT getChildAsValueOrDefault_t(const QString& key, const _ValueT& defaultValue)
  {
     JSONData* child = at(key);
     if (child)
     {
         return child->asValue<_ValueT>();
     }
     else
     {
         return defaultValue;
     }
  }
};

template<>
inline bool JSONData::asValue<bool>() { return asBool(); }

template<>
inline double JSONData::asValue<double>() { return asNumber(); }

template<>
inline QString JSONData::asValue<QString>() { return asString(); }

class JSONBool : public JSONData {
 public:
  explicit JSONBool(bool val);
  bool asBool() override;
 private:
  bool data;
};

class JSONString : public JSONData {
 public:
  explicit JSONString(QString val);
  QString asString() override;
 private:
  QString data;
};

class JSONNumber : public JSONData {
 public:
  explicit JSONNumber(double val);
  double asNumber() override;
 private:
  double data;
};

class JSONObject : public JSONData {
 public:
  explicit JSONObject(JSONHelper &);
  ~JSONObject();
  bool has(const QString key);
  JSONData *at(const QString key);
 private:
  QHash<QString, JSONData *>children;
};
class JSONArray : public JSONData {
 public:
  explicit JSONArray(JSONHelper &);
  ~JSONArray();
  JSONData *at(int index);
  int length();
 private:
  QList<JSONData *>data;
};

class JSONParseException {
 public:
  JSONParseException(QString reason, QString at) :
    reason(QString("%1 at %2").arg(reason).arg(at)) {}
  QString reason;
};

class JSON {
 public:
  static JSONData *parse(const QString data);
};

#endif  // JSON_H_
