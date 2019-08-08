#ifndef SEARCHPLUGININTERFACE_H
#define SEARCHPLUGININTERFACE_H

#include <vector>

class Chunk;

class QWidget;
class SearchResultItem;

class SearchPluginI
{
public:
    virtual QWidget& getWidget() = 0;

    virtual bool initSearch() { return true; }

    using ResultListT = std::vector<SearchResultItem>;

    virtual ResultListT searchChunk(Chunk &chunk) = 0;

    virtual ~SearchPluginI() {}
};

#endif // SEARCHPLUGININTERFACE_H
