#pragma once

// We store all data here.
// If the data is not stored here, it is stored on disk.

#include <shared_mutex>
#include <unordered_map>
#include <lsp/types.h>

#include "Defines.h"
#include "TreeSitterData.h"
#include "hash/Hash.h"

#include "MultiMap.h"

struct FileKey
{
    std::string uriKey;
    Utils::HashCode hashCode;

    bool operator==(const FileKey& rhs) const { return uriKey == rhs.uriKey && hashCode == rhs.hashCode; }
};

template<>
struct std::hash<FileKey>
{
    std::size_t operator()(const FileKey& k) const noexcept
    {
        return std::hash<std::string>()(k.uriKey) ^ (std::hash<Utils::HashCode>()(k.hashCode) << 1);
    }
};

struct FileHeader
{
    Utils::HashCode hash; // Hash of the content
    FileHeader() {}
    FileHeader(FileHeader&& other) : hash(std::move(other.hash)) {}
    bool operator==(const FileHeader& rhs) const { return hash == rhs.hash; }
};

struct FileEntry
{
    Utils::HashCode hash; // Hash of the active defines

    FileEntry() {}
    FileEntry(FileEntry&& other) : hash(std::move(other.hash)) {}
    bool operator==(const FileEntry& rhs) const { return hash == rhs.hash; }
};
template<>
struct std::hash<FileEntry>
{
    std::size_t operator()(const FileEntry& k) const noexcept
    {
        return std::hash<Utils::HashCode>()(k.hash);
    }
};

struct DataHolder
{
    inline static std::shared_mutex s_TreeSitterDataMutex;
    inline static std::unordered_map<std::string, TreeSitterData> s_URIToTreeSitterData;

    inline static std::shared_mutex s_DependencyMapMutex;
    // [include] -> parent
    inline static std::unordered_multimap<FileKey, FileKey> s_DependencyMap;

    inline static MultiMap<std::string, FileHeader, FileEntry> s_MemoryCache;
};

namespace DependencyMap
{
    inline void AddDependency(const FileKey& from, const FileKey& to)
    {

    }
}

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

