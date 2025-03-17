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
    DECLARATION = 0u,
    DEFINITION,
    STATIC,
    COUNT
};
const char* g_SemanticTokenModifiers[] =
{
    "declaration",
    "definition",
    "static"
};

// Used for all communication between server and client.
lsp::MessageHandler* g_pMessageHandler = nullptr;

struct TreeSitterData
{
    TSParser* m_pParser = nullptr;
    TSTree* m_pCurrentTree = nullptr;
    FileBuffer m_FileBuffer;

    TreeSitterData() { InitTreeSitter(); }
    ~TreeSitterData() { DeleteTreeSitter(); }

    void InitTreeSitter()
    {
        m_pParser = ts_parser_new();
        ts_parser_set_language(m_pParser, tree_sitter_hlslvparser());
    }

    void DeleteTreeSitter()
    {
        if (m_pParser == nullptr)
            return;

        if (m_pCurrentTree)
            ts_tree_delete(m_pCurrentTree);
        ts_parser_delete(m_pParser);
    }
};

std::unordered_map<std::string, TreeSitterData> g_URIToTreeSitterData;

TreeSitterData& FetchTreeSitterData(const lsp::FileURI& uri)
{
    // Fetch from memory cache.
    auto it = g_URIToTreeSitterData.find(uri.toString());
    if (it != g_URIToTreeSitterData.end())
        return it->second;

    // Create a new.
    return g_URIToTreeSitterData[uri.toString()];
}

void RemoveTreeSitterData(const lsp::FileURI& uri)
{
    auto it = g_URIToTreeSitterData.find(uri.toString());
    if (it == g_URIToTreeSitterData.end())
        return;

    g_URIToTreeSitterData.erase(it);
}

struct Walker
{
public:
    Walker() { }
    ~Walker() { }

    // text: The text to replace the text in the range.
    // (optional) range: The range of the document that got changed
    void Parse(lsp::FileURI uri, const char* pText, size_t textLength, const lsp::Range* pRange = nullptr)
    {
        TreeSitterData& data = FetchTreeSitterData(uri);

        if (data.m_pParser != nullptr && pRange == nullptr)
            ts_parser_reset(data.m_pParser);

        // An edit occured.
        if (pRange != nullptr && data.m_pCurrentTree != nullptr)
        {
            TSInputEdit inputEdit;
            inputEdit.start_point = { .row = pRange->start.line, .column = pRange->start.character };
            inputEdit.old_end_point = { .row = pRange->end.line, .column = pRange->end.character };
            inputEdit.start_byte = (uint32_t)data.m_FileBuffer.FetchByteOffset(pRange->start);
            inputEdit.old_end_byte = (uint32_t)data.m_FileBuffer.FetchByteOffset(pRange->end);

            lsp::Range newRange = data.m_FileBuffer.ReplaceAt(pText, textLength, *pRange);
            inputEdit.new_end_point = { .row = newRange.end.line, .column = newRange.end.character };
            inputEdit.new_end_byte = (uint32_t)data.m_FileBuffer.FetchByteOffset(newRange.end);
            ts_tree_edit(data.m_pCurrentTree, &inputEdit);
        }
        else
        {
            data.m_FileBuffer.Create(pText, textLength);
        }

        const char* pFullTex = data.m_FileBuffer.CopyText();
        uint32_t fullTextSize = (uint32_t)data.m_FileBuffer.GetLength();
        data.m_pCurrentTree = ts_parser_parse_string(data.m_pParser, data.m_pCurrentTree, pFullTex, fullTextSize);

        std::string debugTreeStr = FetchDebugTreeStr(uri);
        SendLog(debugTreeStr);
    }

    std::string FetchDebugTreeStr(lsp::FileURI uri)
    {
        TreeSitterData& data = FetchTreeSitterData(uri);
        std::string debugStr;
        TSNode rootNode = ts_tree_root_node(data.m_pCurrentTree);
        WalkTree(rootNode, 0, debugStr);
        return debugStr;
    }

private:
    void WalkTree(TSNode node, uint32_t depth, std::string& debugOutput)
    {
        uint32_t childCount = ts_node_child_count(node);

        TSPoint startPoint = ts_node_start_point(node);
        uint32_t startByte = ts_node_start_byte(node);
        TSPoint endPoint = ts_node_end_point(node);
        uint32_t endByte = ts_node_end_byte(node);
        for (uint32_t i = 0; i < depth; ++i)
            debugOutput += " ";
        debugOutput += std::format("{}\t[{}:{} - {}:{}] [{} - {}]\n",
            ts_node_type(node), startPoint.row, startPoint.column, endPoint.row, endPoint.column, startByte, endByte);

        for (uint32_t i = 0; i < childCount; ++i)
        {
            TSNode child = ts_node_child(node, i);
            WalkTree(child, depth+1, debugOutput);
        }
    }
};

#ifdef MSLP_DEBUG

namespace DebugWalker
{
    void UnitTest()
    {
        lsp::FileURI emptyUri;
        Walker walker;

        // Test 1
        {
            const char* text =
                "int a;\r\n"
                "bool b;\r\n"
                "int c = foo();\r\n"
                "if (c)\r\n"
                "\tPrint(\"Error\");\r\n";
            walker.Parse(emptyUri, text, strlen(text));
            std::string debugRes = walker.FetchDebugTreeStr(emptyUri);

            lsp::Range editRange;
            editRange.start = { .line = 3, .character = 0 };
            editRange.end = { .line = 4, .character = 1 };
            walker.Parse(emptyUri, "", 0u, &editRange);
            debugRes = walker.FetchDebugTreeStr(emptyUri);

            RemoveTreeSitterData(emptyUri);
        }

        // Test 2
        {
            const char* text =
                "float foo(float3 v)\r\n"
                "{\r\n"
                "\treturn v.x;\r\n"
                "}\r\n";
            walker.Parse(emptyUri, text, strlen(text));
            std::string debugRes = walker.FetchDebugTreeStr(emptyUri);

            lsp::Range editRange;
            editRange.start = { .line = 0, .character = 5 };
            editRange.end = { .line = 0, .character = 5 };
            walker.Parse(emptyUri, "3", 0u, &editRange);
            debugRes = walker.FetchDebugTreeStr(emptyUri);

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

    Walker walker;

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
    .add<lsp::notifications::TextDocument_DidOpen>([&walker](lsp::DidOpenTextDocumentParams&& params)
        {
            SendMessage(std::format("Opened TextDocument: {}", params.textDocument.uri.path().c_str()));

            const lsp::FileURI& uri = params.textDocument.uri;
            std::string& text = params.textDocument.text;
            walker.Parse(uri, text.c_str(), text.size());
        })
    .add<lsp::notifications::TextDocument_DidChange>([&walker](lsp::DidChangeTextDocumentParams&& params)
        {
            SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

            const lsp::FileURI& uri = params.textDocument.uri;

            for (uint32_t changeIndex = 0; changeIndex < params.contentChanges.size(); ++changeIndex)
            {
                lsp::TextDocumentContentChangeEvent& changeEvent = params.contentChanges[changeIndex];
                if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent))
                {
                    lsp::TextDocumentContentChangeEvent_Range_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent);
                    walker.Parse(uri, textEvent.text.c_str(), textEvent.text.size(), &textEvent.range);
                }
                else
                {
                    lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(changeEvent);
                    walker.Parse(uri, textEvent.text.c_str(), textEvent.text.size());
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
            //result = tokens;

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
