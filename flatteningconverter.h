/** Copyright (c) 2018, EtlamGit */
#ifndef FLATTENINGCONVERTER_H_
#define FLATTENINGCONVERTER_H_

#include "./paletteentry.h"
#include "identifierinterface.h"

class JSONArray;
class JSONObject;


class FlatteningConverter: public IdentifierI {
public:
  // singleton: access to global usable instance
  static FlatteningConverter &Instance();

  int addDefinitions(JSONArray *, int pack = -1) override;
  void setDefinitionsEnabled(int packId, bool enabled) override;
//  const BlockData * getPalette();
  PaletteEntry * getPalette();
  const static int paletteLength = 16*256;  // 4 bit data + 8 bit ID

private:
  // singleton: prevent access to constructor and copyconstructor
  FlatteningConverter();
  ~FlatteningConverter();
  FlatteningConverter(const FlatteningConverter &);
  FlatteningConverter &operator=(const FlatteningConverter &);

  void parseDefinition(JSONObject *block, int *parentID, int pack);
  PaletteEntry palette[paletteLength];
//  QList<QList<BlockInfo*> > packs;
};

#endif  // FLATTENINGCONVERTER_H_
