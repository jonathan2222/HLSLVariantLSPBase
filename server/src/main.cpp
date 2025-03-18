#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

#include <tree_sitter/api.h>
#include "tree_sitter_cpp/tree-sitter-cpp.h"
#include "tree_sitter_hlslv/tree-sitter-hlslvparser.h"

#include <iostream>
#include <format>
#include <variant>
#include <shared_mutex>

#include "GapBuffer.h"
#include "LineTracker.h"
#include "FileBuffer.h"

#include "Logger.h"

enum class SemanticTokenType : uint32_t
{
    NAMESPACE = 0u,
    TYPE,
    STRUCT,
    TYPE_PARAMETER,
    PARAMETER,
    VARIABLE,
    FUNCTION,
    KEYWORD,
    COMMENT,
    STRING,
    NUMBER,
    OPERATOR,
    COUNT
};
const char* g_SemanticTokenTypes[] =
{
    "namespace",
    "type",
    "struct",
    "typeParameter",
    "parameter",
    "variable",
    "function",
    "keyword",
    "comment",
    "string",
    "number",
    "operator"
};

enum class SemanticTokenModifier : uint32_t
{
    NONE            = 0u,
    DECLARATION     = 1 << 0,
    DEFINITION      = 1 << 1,
    STATIC          = 1 << 2,
    COUNT           = 3
};
const uint32_t g_SemanticTokenModfiersCount = 3;
const char* g_SemanticTokenModifiers[] =
{
    "declaration",
    "definition",
    "static"
};

// Used for all communication between server and client.
lsp::MessageHandler* g_pMessageHandler = nullptr;

const TSLanguage* g_Language = tree_sitter_hlslvparser();

struct Parser;
struct TreeSitterData
{
    const bool IsCopy = false;
    TSTree* pTree = nullptr;

    TreeSitterData() { Init(); }
    ~TreeSitterData() { Delete(); }
    
    TreeSitterData(const TreeSitterData& other)
        : IsCopy(true)
    {
        pTree = ts_tree_copy(other.pTree);
    }

private:
    void Init()
    {
        Delete();

        pParser = ts_parser_new();
        ts_parser_set_language(pParser, g_Language);

        pFileBuffer = new FileBuffer();
    }

    void Delete()
    {
        if (pTree)
            ts_tree_delete(pTree);
        pTree = nullptr;

        if (pParser == nullptr)
            return;

        ts_parser_delete(pParser);

        if (pFileBuffer)
            delete pFileBuffer;

        pParser = nullptr;
        pFileBuffer = nullptr;
    }

private:
    friend struct Parser;
    TSParser* pParser = nullptr;
    FileBuffer* pFileBuffer = nullptr;
};

std::shared_mutex g_TreeSitterDataMutex;
#define WriteLocker std::unique_lock<std::shared_mutex>
#define ReadLocker std::shared_lock<std::shared_mutex>
std::unordered_map<std::string, TreeSitterData> g_URIToTreeSitterData;

void RemoveTreeSitterData(const lsp::FileURI& uri)
{
    WriteLocker lock(g_TreeSitterDataMutex);

    auto it = g_URIToTreeSitterData.find(uri.toString());
    if (it == g_URIToTreeSitterData.end())
        return;

    g_URIToTreeSitterData.erase(it);
}

TreeSitterData FetchShallowCopy(const lsp::FileURI& uri)
{
    {
        // Fetch from memory cache.
        WriteLocker lock(g_TreeSitterDataMutex);
        auto it = g_URIToTreeSitterData.find(uri.toString());
        if (it != g_URIToTreeSitterData.end())
            return it->second; // Copy
    }

    assert(false && "Cannot an empty TreeSitterData struct!");
    return {};
}

struct Parser
{
public:
    Parser() { }
    ~Parser() { }

    // text: The text to replace the text in the range.
    // (optional) range: The range of the document that got changed
    void Parse(lsp::FileURI uri, const char* pText, size_t textLength, const lsp::Range* pRange = nullptr)
    {
        TreeSitterData& data = FetchTreeSitterData(uri);

        if (data.pParser != nullptr && pRange == nullptr)
            ts_parser_reset(data.pParser);

        // An edit occured.
        if (pRange != nullptr && data.pTree != nullptr)
        {
            TSInputEdit inputEdit;
            inputEdit.start_point = { .row = pRange->start.line, .column = pRange->start.character };
            inputEdit.old_end_point = { .row = pRange->end.line, .column = pRange->end.character };
            inputEdit.start_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(pRange->start);
            inputEdit.old_end_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(pRange->end);

            lsp::Range newRange = data.pFileBuffer->ReplaceAt(pText, textLength, *pRange);
            inputEdit.new_end_point = { .row = newRange.end.line, .column = newRange.end.character };
            inputEdit.new_end_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(newRange.end);
            ts_tree_edit(data.pTree, &inputEdit);
        }
        else
        {
            data.pFileBuffer->Create(pText, textLength);
        }

        const char* pFullTex = data.pFileBuffer->CopyText();
        uint32_t fullTextSize = (uint32_t)data.pFileBuffer->GetLength();
        data.pTree = ts_parser_parse_string(data.pParser, data.pTree, pFullTex, fullTextSize);

        std::string debugTreeStr = FetchDebugTreeStr(uri);
        SendLog(debugTreeStr);

        TSNode rootNode = ts_tree_root_node(data.pTree);
        char* sExpression = ts_node_string(rootNode);
        SendLog(sExpression);
        delete sExpression;

#ifdef MSLP_DEBUG
        std::string debugFileBufferInfo = data.pFileBuffer->DebugInspect();
        SendLog(debugFileBufferInfo);
#endif
    }

    std::string FetchDebugTreeStr(lsp::FileURI uri)
    {
        TreeSitterData& data = FetchTreeSitterData(uri);
        std::string debugStr;
        TSNode rootNode = ts_tree_root_node(data.pTree);
        WalkTree(rootNode, 0, debugStr, nullptr);
        return debugStr;
    }

private:
    void WalkTree(TSNode node, uint32_t depth, std::string& debugOutput, const char* pField)
    {
        uint32_t childCount = ts_node_child_count(node);

        TSPoint startPoint = ts_node_start_point(node);
        uint32_t startByte = ts_node_start_byte(node);
        TSPoint endPoint = ts_node_end_point(node);
        uint32_t endByte = ts_node_end_byte(node);
        for (uint32_t i = 0; i < depth; ++i)
            debugOutput += " ";
        std::string field;
        if (pField)
            field = std::format(" - {}", pField);
        debugOutput += std::format("{}{}\t[{}:{} - {}:{}] [{} - {}]\n",
            ts_node_type(node), field.c_str(),
            startPoint.row, startPoint.column, endPoint.row, endPoint.column, startByte, endByte);
        for (uint32_t i = 0; i < childCount; ++i)
        {
            const char* pChildField = ts_node_field_name_for_child(node, i);
            TSNode child = ts_node_child(node, i);
            WalkTree(child, depth+1, debugOutput, pChildField);
        }
    }

    TreeSitterData& FetchTreeSitterData(const lsp::FileURI& uri)
    {
        {
            // Fetch from memory cache.
            ReadLocker lock(g_TreeSitterDataMutex);
            auto it = g_URIToTreeSitterData.find(uri.toString());
            if (it != g_URIToTreeSitterData.end())
                return it->second;
        }
        {
            // Create a new.
            WriteLocker lock(g_TreeSitterDataMutex);
            return g_URIToTreeSitterData[uri.toString()];
        }
    }
};

#ifdef MSLP_DEBUG

namespace DebugWalker
{
    void UnitTest()
    {
        lsp::FileURI emptyUri;
        Parser parser;

        // Test 1
        {
            const char* text =
                "int a;\r\n"
                "bool b;\r\n"
                "int c = foo();\r\n"
                "if (c)\r\n"
                "\tPrint(\"Error\");\r\n";
            parser.Parse(emptyUri, text, strlen(text));
            std::string debugRes = parser.FetchDebugTreeStr(emptyUri);

            lsp::Range editRange;
            editRange.start = { .line = 3, .character = 0 };
            editRange.end = { .line = 4, .character = 1 };
            parser.Parse(emptyUri, "", 0u, &editRange);
            debugRes = parser.FetchDebugTreeStr(emptyUri);

            RemoveTreeSitterData(emptyUri);
        }

        // Test 2
        {
            const char* text =
                "float foo(float3 v)\r\n"
                "{\r\n"
                "\treturn v.x;\r\n"
                "}\r\n";
            parser.Parse(emptyUri, text, strlen(text));
            std::string debugRes = parser.FetchDebugTreeStr(emptyUri);

            lsp::Range editRange;
            editRange.start = { .line = 0, .character = 5 };
            editRange.end = { .line = 0, .character = 5 };
            parser.Parse(emptyUri, "3", 0u, &editRange);
            debugRes = parser.FetchDebugTreeStr(emptyUri);

            RemoveTreeSitterData(emptyUri);
        }
    }
}

#endif

int main()
{
    std::string bufferStr, scratchStr;

#ifdef MSLP_DEBUG
    DebugGapBuffer::UnitTest();
    DebugLineTracker::UnitTest();
    //DebugWalker::UnitTest();
#endif

    Parser parser;

    // 1: Establish a connection using standard input/output
    lsp::Connection connection{ lsp::io::standardInput(), lsp::io::standardOutput() };

    // 2: Create a MessageHandler with the connection
    g_pMessageHandler = new lsp::MessageHandler(connection);

    bool running = true;

    // 3: Register callbacks for incoming messages
    g_pMessageHandler->requestHandler()
    // Request callbacks always have the message id as the first parameter followed by the params if there are any.
    .add<lsp::requests::Initialize>([](const lsp::jsonrpc::MessageId& /*id*/, lsp::requests::Initialize::Params&& params)
        {
            lsp::requests::Initialize::Result result;
            // Initialize the result and return it or throw an lsp::RequestError if there was a problem
            // Alternatively do processing asynchronously and return a std::future here
            lsp::TextDocumentSyncOptions syncOptions;
            syncOptions.openClose = true;
            syncOptions.change = lsp::TextDocumentSyncKind::Incremental;
            result.capabilities.textDocumentSync = syncOptions;

            lsp::SemanticTokensOptions semanticTokensOptions;
            semanticTokensOptions.legend.tokenTypes.reserve((uint32_t)SemanticTokenType::COUNT);
            for (uint32_t i = 0u; i < (uint32_t)SemanticTokenType::COUNT; ++i)
                semanticTokensOptions.legend.tokenTypes.push_back(g_SemanticTokenTypes[i]);
            semanticTokensOptions.legend.tokenModifiers.reserve((uint32_t)SemanticTokenModifier::COUNT);
            for (uint32_t i = 0u; i < (uint32_t)SemanticTokenModifier::COUNT; ++i)
                semanticTokensOptions.legend.tokenModifiers.push_back(g_SemanticTokenModifiers[i]);
            semanticTokensOptions.full = lsp::SemanticTokensOptionsFull();
            result.capabilities.semanticTokensProvider = semanticTokensOptions;

            lsp::ClientCapabilities clientCapabilities = params.capabilities;
            if (clientCapabilities.textDocument.has_value()) // Weird if the client does not support this.
            {
                lsp::TextDocumentClientCapabilities textDocClientCapabilities = clientCapabilities.textDocument.value();
                if (textDocClientCapabilities.semanticTokens.has_value())
                {
                    lsp::SemanticTokensClientCapabilities semanticTokensClientCapabilities = textDocClientCapabilities.semanticTokens.value();
                    lsp::SemanticTokensClientCapabilitiesRequests requests = semanticTokensClientCapabilities.requests;
                    if (requests.range.has_value())
                    {

                    }
                    if (requests.full.has_value())
                    {

                    }
                    else
                    {
                        // Error
                        // We only support full semantic tokens request.
                        SendMessageError("Server only supports Semantic tokens Full request client does not: Semantic tokens will be ignored!");
                    }
                }
            }

            SendMessage("Testing LSP V2");

            return result;
        })
    // Notifications don't have an id parameter because no response is sent back for them.
    .add<lsp::notifications::Exit>([&running]()
        {
            running = false;
        })
    .add<lsp::notifications::TextDocument_DidOpen>([&parser](lsp::DidOpenTextDocumentParams&& params)
        {
            SendMessage(std::format("Opened TextDocument: {}", params.textDocument.uri.path().c_str()));

            const lsp::FileURI& uri = params.textDocument.uri;
            std::string& text = params.textDocument.text;
            parser.Parse(uri, text.c_str(), text.size());
        })
    .add<lsp::notifications::TextDocument_DidChange>([&parser](lsp::DidChangeTextDocumentParams&& params)
        {
            SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

            const lsp::FileURI& uri = params.textDocument.uri;

            for (uint32_t changeIndex = 0; changeIndex < params.contentChanges.size(); ++changeIndex)
            {
                lsp::TextDocumentContentChangeEvent& changeEvent = params.contentChanges[changeIndex];
                if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent))
                {
                    lsp::TextDocumentContentChangeEvent_Range_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent);
                    parser.Parse(uri, textEvent.text.c_str(), textEvent.text.size(), &textEvent.range);
                }
                else
                {
                    lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(changeEvent);
                    parser.Parse(uri, textEvent.text.c_str(), textEvent.text.size());
                }
            }
        })
    .add<lsp::notifications::TextDocument_DidClose>([](lsp::DidCloseTextDocumentParams&& params)
        {
            SendMessage(std::format("Closed TextDocument: {}", params.textDocument.uri.path().c_str()));
        })
    .add<lsp::requests::TextDocument_SemanticTokens_Full>([](const lsp::jsonrpc::MessageId& /*id*/,
        lsp::requests::TextDocument_SemanticTokens_Full::Params&& params)
        {
            lsp::requests::TextDocument_SemanticTokens_Full::Result result;

            lsp::SemanticTokens tokens;
            TreeSitterData tsData = FetchShallowCopy(params.textDocument.uri);

            // This is not the best way of coloring, because we would like to color more than just the types,
            // and we do not want to go through the tree multiple times.
            // Meaning we can go through it ourself instead of using queries. But this if fine for now.
            const char* pQuerySource = "[(type_identifier) @ti\n(primitive_type) @tp]";
            uint32_t errorOffset = 0u;
            TSQueryError errorType;
            TSQuery* pQuery = ts_query_new(g_Language, pQuerySource, strlen(pQuerySource), &errorOffset, &errorType);

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
        });

    // 4: Start the message processing loop
    // processIncomingMessages Reads all current messages from the connection and if there are none waits until one becomes available
    try
    {
        while (running)
            g_pMessageHandler->processIncomingMessages();
    }
    catch (lsp::ConnectionError e)
    {
        // Lost connection
        //e.what();
    }

    //std::cout << "Server stopped" << std::endl;
    return 0;
}
