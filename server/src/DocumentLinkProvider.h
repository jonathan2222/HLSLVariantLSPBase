#pragma once

#include <lsp/messages.h>
#include <lsp/jsonrpc/jsonrpc.h>

#include <tree_sitter/api.h>

#include <filesystem>
#include <format>

#include "TreeSitterData.h"
#include "DataHolder.h"
#include "StringUtils.h"
#include "Logger.h"

struct DocumentLinkProvider
{
	static lsp::requests::TextDocument_DocumentLink::Result Provider(const lsp::jsonrpc::MessageId& /*id*/,
		lsp::requests::TextDocument_DocumentLink::Params&& params)
	{
        lsp::requests::TextDocument_DocumentLink::Result result;

        std::vector<lsp::DocumentLink> documentLinks;
        TreeSitterData tsData = TreeSitterDataUser::FetchShallowCopy(params.textDocument.uri);

        // Query is perfect for finding links.
        //const char* pQuerySource = "(preproc_include (string_literal (string_content) @link))";
        const char* pQuerySource = "(string_content) @link";
        uint32_t errorOffset = 0u;
        TSQueryError errorType;
        TSQuery* pQuery = ts_query_new(TreeSitterData::pLanguage, pQuerySource, strlen(pQuerySource), &errorOffset, &errorType);

        if (pQuery)
        {
            std::string_view thisFilePath = params.textDocument.uri.path();
            std::vector<std::string_view> thisFileParts = Utils::Split(thisFilePath, '/');
            tsData.BeginReading();

            TSNode rootNode = ts_tree_root_node(tsData.pTree);
            TSQueryCursor* pCursor = ts_query_cursor_new();
            ts_query_cursor_exec(pCursor, pQuery, rootNode);

            uint32_t captureIndex;
            TSQueryMatch queryMatch;
            while (ts_query_cursor_next_capture(pCursor, &queryMatch, &captureIndex))
            {
                // capture.index is the capture index in the query.
                const TSQueryCapture& capture = queryMatch.captures[captureIndex];
                uint32_t startByte = ts_node_start_byte(capture.node);
                uint32_t endByte = ts_node_end_byte(capture.node);
                TSPoint startPoint = ts_node_start_point(capture.node);
                TSPoint endPoint = ts_node_end_point(capture.node);

                const std::string_view stringConstant = tsData.ReadString(startByte, endByte - startByte);
                const std::filesystem::path stringConstantAsPath = stringConstant;
                // Check if the extension is valid
                {
                    if (!stringConstantAsPath.has_extension())
                    {
                        SendLogError(std::format("Include path must have an extension!"));
                        continue;
                    }
                    static const wchar_t* validExtensions[2] = { L".hlslv", L".hlsl" };
                    bool hasValidExtension = false;
                    std::filesystem::path ext = stringConstantAsPath.extension();
                    const wchar_t* extension = ext.c_str();
                    for (uint8_t i = 0u; i < 2u; ++i)
                    {
                        if (wcscmp(extension, validExtensions[i]) == 0)
                            hasValidExtension = true;
                    }

                    if (!hasValidExtension)
                    {
                        SendLogError(std::format("Path missing valid extension!"));
                        continue;
                    }
                }

                std::string resolvedPath = Utils::GetPathFromRelativePath(std::string_view(stringConstant), params.textDocument.uri);

                if (resolvedPath.empty())
                {
                    SendLogError(std::format("File does not exist!"));
                    continue;
                }

                lsp::DocumentLink documentLink;
                documentLink.range.start = { .line = startPoint.row, .character = startPoint.column };
                documentLink.range.end = { .line = endPoint.row, .character = endPoint.column };
                documentLink.target = lsp::FileURI(resolvedPath);
                documentLink.tooltip = resolvedPath;
                documentLinks.push_back(documentLink);
            }
            tsData.EndReading();

            ts_query_cursor_delete(pCursor);
            ts_query_delete(pQuery);
        }

        if (documentLinks.empty() == false)
            result = documentLinks;
        return result;
	}
};