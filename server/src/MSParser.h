#pragma once

#include <lsp/types.h>
#include <tree_sitter/api.h>

#include "Defines.h"
#include "TreeSitterData.h"
#include "DataHolder.h"

struct MSParser
{
    struct ParseRequest
    {
        lsp::FileURI uri;
        const char* pText = nullptr;
        size_t textLength;
        const lsp::Range* pRange = nullptr;
    };

    static void RequestParsing(lsp::FileURI uri, const char* pText, size_t textLength, const lsp::Range* pRange = nullptr)
    {

    }

    // text: The text to replace the text in the range.
    // (optional) range: The range of the document that got changed
    static void Parse(lsp::FileURI uri, const char* pText, size_t textLength, const lsp::Range* pRange = nullptr);

    static std::string FetchDebugTreeStr(lsp::FileURI uri)
    {
        TreeSitterData& data = FetchTreeSitterData(uri);
        std::string debugStr;
        TSNode rootNode = ts_tree_root_node(data.pTree);
        WalkTree(rootNode, 0, debugStr, nullptr);
        return debugStr;
    }

private:

    static void WalkTree(TSNode node, uint32_t depth, std::string& debugOutput, const char* pField);

    static TreeSitterData& FetchTreeSitterData(const lsp::FileURI& uri)
    {
        {
            // Fetch from memory cache.
            ReadLocker lock(DataHolder::s_TreeSitterDataMutex);
            auto it = DataHolder::s_URIToTreeSitterData.find(uri.toString());
            if (it != DataHolder::s_URIToTreeSitterData.end())
                return it->second;
        }
        {
            // Create a new.
            WriteLocker lock(DataHolder::s_TreeSitterDataMutex);
            return DataHolder::s_URIToTreeSitterData[uri.toString()];
        }
    }

public:
#ifdef MSLP_DEBUG
    static void UnitTest();
#endif
};

