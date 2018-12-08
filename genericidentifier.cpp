#include "genericidentifier.h"

#include "./json.h"

GenericIdentifier::GenericIdentifier()
{

}

GenericIdentifier::~GenericIdentifier()
{
    for (int i = 0; i < packs.length(); i++) {
      for (int j = 0; j < packs[i].length(); j++)
        delete packs[i][j];
    }
}

int GenericIdentifier::addDefinitions(JSONArray *defs, int pack)
{
    if (pack == -1) {
      pack = packs.length();
      packs.append(QList<GenericDescriptor *>());
    }

    int len = defs->length();
    for (int i = 0; i < len; i++) {
      JSONObject *b = dynamic_cast<JSONObject *>(defs->at(i));
      auto idTag = b->at("id");
      int id = static_cast<int>(idTag->asNumber());

      GenericDescriptor *biome = new GenericDescriptor();
      biome->enabled = true;
      biome->id = id;
      if (b->has("name"))
        biome->name = b->at("name")->asString();

      biomes[id].append(biome);
      packs[pack].append(biome);
    }

    return pack;
}

void GenericIdentifier::setDefinitionsEnabled(int pack, bool enabled)
{
    if (pack < 0) return;
    int len = packs[pack].length();
    for (int i = 0; i < len; i++)
        packs[pack][i]->enabled = enabled;
}

static GenericDescriptor s_unknown;

GenericDescriptor &GenericIdentifier::getDescriptor(int id)
{
    QList<GenericDescriptor*> &list = biomes[id];
    // search backwards for priority sorting to work
    for (int i = list.length() - 1; i >= 0; i--)
      if (list[i]->enabled)
        return *list[i];
    return s_unknown;
}

QList<int> GenericIdentifier::getKnownIds() const
{
    return biomes.keys();
}
