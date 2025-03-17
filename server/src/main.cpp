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

#include "StringUtils.h"

void _SendMessage(lsp::MessageHandler& messageHandler, const std::string& message)
{
    lsp::ShowMessageRequestParams messageParams;
        messageParams.type = lsp::MessageType::Info;
        messageParams.message = message;
        /*messageParams.actions = { lsp::MessageActionItem{"Title Test"} };*/
        lsp::jsonrpc::MessageId messageId = messageHandler.messageDispatcher().sendRequest<lsp::requests::Window_ShowMessageRequest>(
        lsp::requests::Window_ShowMessageRequest::Params{ messageParams },
        [](lsp::requests::Window_ShowMessageRequest::Result&& /*result*/) {},
        [](const lsp::Error& /*error*/) {});
}
#define SendMessage(msg) _SendMessage(*g_pMessageHandler, msg)

void _SendLog(lsp::MessageHandler& messageHandler, const std::string& message)
{
    lsp::LogMessageParams messageParams;
    messageParams.type = lsp::MessageType::Info;
    messageParams.message = message;
    messageHandler.messageDispatcher().sendNotification<lsp::notifications::Window_LogMessage>(lsp::notifications::Window_LogMessage::Params{ messageParams });
}
#define SendLog(msg) _SendLog(*g_pMessageHandler, msg)

// Used for all communication between server and client.
lsp::MessageHandler* g_pMessageHandler = nullptr;

struct Walker
{
private:
    TSParser* m_pParser = nullptr;
    TSTree* m_pCurrentTree = nullptr;
    FileBuffer m_FileBuffer;

public:
    Walker() { InitTreeSitter(); }
    ~Walker() { DeleteTreeSitter(); }

    // text: The text to replace the text in the range.
    // (optional) range: The range of the document that got changed
    void Parse(const char* pText, size_t textLength, const lsp::Range* pRange = nullptr)
    {
        if (m_pParser != nullptr && pRange == nullptr)
            ts_parser_reset(m_pParser);

        // An edit occured.
        if (pRange != nullptr && m_pCurrentTree != nullptr)
        {
            TSInputEdit inputEdit;
            inputEdit.start_point = { .row = pRange->start.line, .column = pRange->start.character };
            inputEdit.old_end_point = { .row = pRange->end.line, .column = pRange->end.character };
            inputEdit.start_byte = (uint32_t)m_FileBuffer.FetchByteOffset(pRange->start);
            inputEdit.old_end_byte = (uint32_t)m_FileBuffer.FetchByteOffset(pRange->end);

            lsp::Range newRange = m_FileBuffer.ReplaceAt(pText, textLength, *pRange);
            inputEdit.new_end_point = { .row = newRange.end.line, .column = newRange.end.character };
            inputEdit.new_end_byte = (uint32_t)m_FileBuffer.FetchByteOffset(newRange.end);
            ts_tree_edit(m_pCurrentTree, &inputEdit);
        }
        else
        {
            m_FileBuffer.Create(pText, textLength);
        }

        const char* pFullTex = m_FileBuffer.CopyText();
        uint32_t fullTextSize = (uint32_t)m_FileBuffer.GetLength();
        m_pCurrentTree = ts_parser_parse_string(m_pParser, m_pCurrentTree, pFullTex, fullTextSize);

        std::string debugTreeStr = FetchDebugTreeStr();
        SendLog(debugTreeStr);
    }

    TSParser* GetParser() { return m_pParser; }

    std::string GetTreeString()
    {
        TSNode rootNode = ts_tree_root_node(m_pCurrentTree);
        char* pString = ts_node_string(rootNode);
        std::string msg = pString;
        free(pString);
        return msg;
    }

    std::string FetchDebugTreeStr()
    {
        std::string debugStr;
        TSNode rootNode = ts_tree_root_node(m_pCurrentTree);
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

#ifdef MSLP_DEBUG

namespace DebugWalker
{
    void UnitTest()
    {
        // Test 1
        {
            Walker walker;
            const char* text =
                "int a;\r\n"
                "bool b;\r\n"
                "int c = foo();\r\n"
                "if (c)\r\n"
                "\tPrint(\"Error\");\r\n";
            walker.Parse(text, strlen(text));
            std::string result = walker.GetTreeString();
            std::string debugRes = walker.FetchDebugTreeStr();

            lsp::Range editRange;
            editRange.start = { .line = 3, .character = 0 };
            editRange.end = { .line = 4, .character = 1 };
            walker.Parse("", 0u, &editRange);
            result = walker.GetTreeString();
            debugRes = walker.FetchDebugTreeStr();

            result = "";
        }

        // Test 2
        {
            Walker walker;
            const char* text =
                "float foo(float3 v)\r\n"
                "{\r\n"
                "\treturn v.x;\r\n"
                "}\r\n";
            walker.Parse(text, strlen(text));
            std::string result = walker.GetTreeString();

            lsp::Range editRange;
            editRange.start = { .line = 0, .character = 5 };
            editRange.end = { .line = 0, .character = 5 };
            walker.Parse("3", 0u, &editRange);
            result = walker.GetTreeString();

            result = "";
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
        .add<lsp::requests::Initialize>([](const lsp::jsonrpc::MessageId& /*id*/, lsp::requests::Initialize::Params&& /*params*/)
            {
                lsp::requests::Initialize::Result result;
                // Initialize the result and return it or throw an lsp::RequestError if there was a problem
                // Alternatively do processing asynchronously and return a std::future here
                lsp::TextDocumentSyncOptions syncOptions;
                syncOptions.openClose = true;
                syncOptions.change = lsp::TextDocumentSyncKind::Incremental;
                result.capabilities.textDocumentSync = syncOptions;

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

                std::string& text = params.textDocument.text;
                walker.Parse(text.c_str(), text.size());
            })
        .add<lsp::notifications::TextDocument_DidChange>([&walker](lsp::DidChangeTextDocumentParams&& params)
            {
                SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

                for (uint32_t changeIndex = 0; changeIndex < params.contentChanges.size(); ++changeIndex)
                {
                    lsp::TextDocumentContentChangeEvent& changeEvent = params.contentChanges[changeIndex];
                    if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent))
                    {
                        lsp::TextDocumentContentChangeEvent_Range_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent);
                        walker.Parse(textEvent.text.c_str(), textEvent.text.size(), &textEvent.range);
                    }
                    else
                    {
                        lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(changeEvent);
                        walker.Parse(textEvent.text.c_str(), textEvent.text.size());
                    }
                }
            })
        .add<lsp::notifications::TextDocument_DidClose>([](lsp::DidCloseTextDocumentParams&& params)
            {
                SendMessage(std::format("Closed TextDocument: {}", params.textDocument.uri.path().c_str()));
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
