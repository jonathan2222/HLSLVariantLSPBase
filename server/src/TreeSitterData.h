#pragma once

#include <tree_sitter/api.h>
#include "tree_sitter_hlslv/tree-sitter-hlslvparser.h"

#include <string_view>
#include <shared_mutex>

#include "FileBuffer.h"

struct MSParser;
struct TreeSitterData
{
    inline static const TSLanguage* pLanguage = tree_sitter_hlslvparser();

    const bool IsCopy = false;
    TSTree* pTree = nullptr;

    TreeSitterData() { Init(); }
    ~TreeSitterData() { Delete(); }

    TreeSitterData(const TreeSitterData& other)
        : IsCopy(true)
        , pTextBuffer(other.pTextBuffer)
        , pTextBufferMutex(other.pTextBufferMutex)
    {
        pTree = ts_tree_copy(other.pTree);
    }

    void BeginReading()
    {
        pTextBufferMutex->lock_shared();
    }

    const std::string_view ReadString(uint32_t index, size_t length) const
    {
        return std::string_view(pTextBuffer + index, length);
    }

    void EndReading()
    {
        pTextBufferMutex->unlock_shared();
    }

private:
    void Init()
    {
        Delete();

        pParser = ts_parser_new();
        ts_parser_set_language(pParser, pLanguage);

        pTextBufferMutex = new std::shared_mutex();
        pFileBuffer = new FileBuffer();
    }

    void Delete()
    {
        if (pTree)
            ts_tree_delete(pTree);
        pTree = nullptr;

        if (IsCopy)
            return;

        if (pParser != nullptr)
            ts_parser_delete(pParser);

        if (pTextBuffer)
            delete[] pTextBuffer;
        if (pTextBufferMutex)
            delete pTextBufferMutex;
        if (pFileBuffer)
            delete pFileBuffer;

        pParser = nullptr;
        pFileBuffer = nullptr;
        pTextBufferMutex = nullptr;
        pTextBuffer = nullptr;
    }

private:
    friend struct MSParser;
    TSParser* pParser = nullptr;
    std::shared_mutex* pTextBufferMutex = nullptr;
    const char* pTextBuffer = nullptr;
    FileBuffer* pFileBuffer = nullptr;
};
