#pragma once

// We store all data here.
// If the data is not stored here, it is stored on disk.

#include <shared_mutex>
#include <unordered_map>
#include <lsp/types.h>

#include "Defines.h"
#include "TreeSitterData.h"

struct DataHolder
{
    inline static std::shared_mutex s_TreeSitterDataMutex;
    inline static std::unordered_map<std::string, TreeSitterData> s_URIToTreeSitterData;
};

namespace TreeSitterDataUser
{
    // TODO: Move these functions! Should not be in the data holder!
    inline void RemoveTreeSitterData(const lsp::FileURI& uri)
    {
        WriteLocker lock(DataHolder::s_TreeSitterDataMutex);

        auto it = DataHolder::s_URIToTreeSitterData.find(uri.toString());
        if (it == DataHolder::s_URIToTreeSitterData.end())
            return;

        DataHolder::s_URIToTreeSitterData.erase(it);
    }

    inline TreeSitterData FetchShallowCopy(const lsp::FileURI& uri)
    {
        {
            // Fetch from memory cache.
            WriteLocker lock(DataHolder::s_TreeSitterDataMutex);
            auto it = DataHolder::s_URIToTreeSitterData.find(uri.toString());
            if (it != DataHolder::s_URIToTreeSitterData.end())
                return it->second; // Copy
        }

        assert(false && "Cannot an empty TreeSitterData struct!");
        return {};
    }
}

