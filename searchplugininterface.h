#ifndef SEARCHPLUGININTERFACE_H
#define SEARCHPLUGININTERFACE_H

class SearchResultWidget;
class Chunk;

class QWidget;

class SearchPluginI
{
public:
    virtual QWidget& getWidget() = 0;

    virtual bool initSearch() { return true; }
    virtual void searchChunk(SearchResultWidget &resultList, Chunk &chunk) = 0;

    virtual ~SearchPluginI() {}
};

#endif // SEARCHPLUGININTERFACE_H
