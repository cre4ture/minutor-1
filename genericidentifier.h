#ifndef GENERICIDENTIFIER_H
#define GENERICIDENTIFIER_H

#include "identifierinterface.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QColor>
class JSONArray;
class JSONObject;

struct GenericDescriptor
{
    GenericDescriptor()
        : data(nullptr)
        , enabled(true)
        , id(-1)
        , name("unknown")
    {}

    JSONObject* data;
    bool enabled;

    int id;
    QString name;
};

class GenericIdentifier: public IdentifierI
{
 public:
  GenericIdentifier();
  virtual ~GenericIdentifier() override;
  virtual int addDefinitions(JSONArray *defs, int pack = -1) override;
  virtual void setDefinitionsEnabled(int pack, bool enabled) override;
  GenericDescriptor &getDescriptor(int id);
  QList<int> getKnownIds() const;
 private:
  QHash<int, QList<GenericDescriptor*>> biomes;
  QList<QList<GenericDescriptor*> > packs;
};

#endif // GENERICIDENTIFIER_H
