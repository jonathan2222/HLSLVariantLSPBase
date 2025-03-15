#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

#include <tree_sitter/api.h>
#include "tree_sitter_cpp/tree-sitter-cpp.h"
#include "tree_sitter_hlslv/tree-sitter-hlslvparser.h"

#include <iostream>
#include <format>

#include "GapBuffer.h"

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
public:
    Walker() { InitTreeSitter(); }
    ~Walker() { DeleteTreeSitter(); }

    // range: The range of the document that got changed
    // text: The text to replace the text in the range.
    void Parse(const lsp::Range& range, const std::string& text)
    {
        TSTree* pTree = ts_parser_parse_string(m_pParser, m_pCurrentTree, text.c_str(), (uint32_t)text.size());
        TSNode rootNode = ts_tree_root_node(pTree);
        char* pString = ts_node_string(rootNode);
        std::string msg = pString;
        SendLog(msg);
        free(pString);
        ts_tree_delete(pTree);
    }

    TSParser* GetParser() { return m_pParser; }

private:
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

int main()
{
    GapBuffer gapBuffer;
    gapBuffer.Create(,);


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
                syncOptions.change = lsp::TextDocumentSyncKind::Full;
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
                TSTree* pTree = ts_parser_parse_string(walker.GetParser(), NULL, text.c_str(), (uint32_t)text.size());
                TSNode rootNode = ts_tree_root_node(pTree);
                char* pString = ts_node_string(rootNode);
                std::string msg = pString;
                SendLog(msg);
                free(pString);
                ts_tree_delete(pTree);
            })
        .add<lsp::notifications::TextDocument_DidChange>([&walker](lsp::DidChangeTextDocumentParams&& params)
            {
                SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

                lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(params.contentChanges[0]);
                TSTree* pTree = ts_parser_parse_string(walker.GetParser(), NULL, textEvent.text.c_str(), (uint32_t)textEvent.text.size());

                TSNode rootNode = ts_tree_root_node(pTree);
                char* pString = ts_node_string(rootNode);
                std::string msg = pString;
                SendLog(msg);
                free(pString);
                ts_tree_delete(pTree);
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
