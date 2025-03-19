#pragma once

#include <lsp/messages.h>
#include <lsp/jsonrpc/jsonrpc.h>

#include "DataHolder.h"
#include "TreeSitterData.h"
#include "SemanticTokens.h"

struct SemanticTokensProvider
{
	static lsp::requests::TextDocument_SemanticTokens_Full::Result Provider(const lsp::jsonrpc::MessageId& /*id*/,
		lsp::requests::TextDocument_SemanticTokens_Full::Params&& params)
	{
        lsp::requests::TextDocument_SemanticTokens_Full::Result result;

        lsp::SemanticTokens tokens;
        TreeSitterData tsData = TreeSitterDataUser::FetchShallowCopy(params.textDocument.uri);

        // This is not the best way of coloring, because we would like to color more than just the types,
        // and we do not want to go through the tree multiple times.
        // Meaning we can go through it ourself instead of using queries. But this if fine for now.
        const char* pQuerySource = "[(type_identifier) @ti\n(primitive_type) @tp]";
        uint32_t errorOffset = 0u;
        TSQueryError errorType;
        TSQuery* pQuery = ts_query_new(TreeSitterData::pLanguage, pQuerySource, strlen(pQuerySource), &errorOffset, &errorType);

        uint32_t prevLineIndex = 0u;
        uint32_t prevCharacterIndex = 0u;
        if (pQuery)
        {
            TSNode rootNode = ts_tree_root_node(tsData.pTree);
            TSQueryCursor* pCursor = ts_query_cursor_new();
            ts_query_cursor_exec(pCursor, pQuery, rootNode);

            uint32_t captureIndex;
            TSQueryMatch queryMatch;
            while (ts_query_cursor_next_capture(pCursor, &queryMatch, &captureIndex))
            {
                // capture.index is the capture index in the query.
                const TSQueryCapture& capture = queryMatch.captures[captureIndex];
                TSPoint startPoint = ts_node_start_point(capture.node);
                TSPoint endPoint = ts_node_end_point(capture.node);

                // One token (Assume no multiline)
                uint32_t lineDelta = startPoint.row - prevLineIndex;
                uint32_t charDelta = (startPoint.row == prevLineIndex) ? startPoint.column - prevCharacterIndex : startPoint.column;
                tokens.data.push_back(lineDelta);      // Line
                tokens.data.push_back(charDelta);   // Character
                tokens.data.push_back(endPoint.column - startPoint.column); // Length
                tokens.data.push_back((uint32_t)SemanticTokenType::TYPE);
                tokens.data.push_back((uint32_t)SemanticTokenModifier::NONE);

                // Used for the 'relative' format
                prevLineIndex = startPoint.row;
                prevCharacterIndex = startPoint.column;
            }

            ts_query_cursor_delete(pCursor);
            ts_query_delete(pQuery);
        }

#ifdef MSLP_DEBUG
        if (false)
        {
            // Test token
            tokens.data.push_back(0);  // Line
            tokens.data.push_back(0);  // Character
            tokens.data.push_back(5); // Length
            tokens.data.push_back((uint32_t)SemanticTokenType::TYPE);
            tokens.data.push_back((uint32_t)SemanticTokenModifier::NONE);

            // Another test token
            tokens.data.push_back(1);  // Line
            tokens.data.push_back(0);  // Character
            tokens.data.push_back(5); // Length
            tokens.data.push_back((uint32_t)SemanticTokenType::TYPE);
            tokens.data.push_back((uint32_t)SemanticTokenModifier::NONE);

            // Third test token
            tokens.data.push_back(1);  // Line
            tokens.data.push_back(0);  // Character
            tokens.data.push_back(5); // Length
            tokens.data.push_back((uint32_t)SemanticTokenType::TYPE);
            tokens.data.push_back((uint32_t)SemanticTokenModifier::NONE);
        }
#endif

        if (tokens.data.empty() == false)
            result = tokens;
        return result;
	}
};