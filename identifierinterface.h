#ifndef IDENTIFIERINTERFACE_H
#define IDENTIFIERINTERFACE_H

class JSONArray;

class IdentifierI
{
public:
    virtual int addDefinitions(JSONArray *defs, int pack = -1) = 0;
    virtual void setDefinitionsEnabled(int packId, bool enabled) = 0;

    virtual ~IdentifierI() {}
};

#endif // IDENTIFIERINTERFACE_H
