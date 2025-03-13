#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

#include <tree_sitter/api.h>
#include "tree_sitter_cpp/tree-sitter-cpp.h"
#include "tree_sitter_hlslv/tree-sitter-hlslvparser.h"

#include <iostream>
#include <format>

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
#define SendMessage(msg) _SendMessage(messageHandler, msg)

void _SendLog(lsp::MessageHandler& messageHandler, const std::string& message)
{
    lsp::LogMessageParams messageParams;
    messageParams.type = lsp::MessageType::Info;
    messageParams.message = message;
    messageHandler.messageDispatcher().sendNotification<lsp::notifications::Window_LogMessage>(lsp::notifications::Window_LogMessage::Params{ messageParams });
}
#define SendLog(msg) _SendLog(messageHandler, msg)

TSParser* g_pParser = nullptr;

void InitTreeSitter()
{
    g_pParser = ts_parser_new();
    ts_parser_set_language(g_pParser, tree_sitter_hlslvparser());
}

void DeleteTreeSitter()
{
    ts_parser_delete(g_pParser);
}

int main()
{
    //std::cout << "Server started" << std::endl;

    InitTreeSitter();

    // 1: Establish a connection using standard input/output
    lsp::Connection connection{ lsp::io::standardInput(), lsp::io::standardOutput() };

    // 2: Create a MessageHandler with the connection
    lsp::MessageHandler messageHandler{ connection };

    bool running = true;

    // 3: Register callbacks for incoming messages
    messageHandler.requestHandler()
        // Request callbacks always have the message id as the first parameter followed by the params if there are any.
        .add<lsp::requests::Initialize>([&messageHandler](const lsp::jsonrpc::MessageId& /*id*/, lsp::requests::Initialize::Params&& /*params*/)
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
        .add<lsp::notifications::TextDocument_DidOpen>([&messageHandler](lsp::DidOpenTextDocumentParams&& params)
            {
                SendMessage(std::format("Opened TextDocument: {}", params.textDocument.uri.path().c_str()));
            })
        .add<lsp::notifications::TextDocument_DidChange>([&messageHandler](lsp::DidChangeTextDocumentParams&& params)
            {
                SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

                lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(params.contentChanges[0]);
                TSTree* pTree = ts_parser_parse_string(g_pParser, NULL, textEvent.text.c_str(), (uint32_t)textEvent.text.size());

                TSNode rootNode = ts_tree_root_node(pTree);
                char* pString = ts_node_string(rootNode);
                std::string msg = pString;
                SendLog(msg);
                free(pString);
                ts_tree_delete(pTree);
            })
        .add<lsp::notifications::TextDocument_DidClose>([&messageHandler](lsp::DidCloseTextDocumentParams&& params)
            {
                SendMessage(std::format("Closed TextDocument: {}", params.textDocument.uri.path().c_str()));
            });

    // 4: Start the message processing loop
    // processIncomingMessages Reads all current messages from the connection and if there are none waits until one becomes available
    try
    {
        while (running)
            messageHandler.processIncomingMessages();
    }
    catch (lsp::ConnectionError e)
    {
        // Lost connection
        //e.what();
    }

    DeleteTreeSitter();

    //std::cout << "Server stopped" << std::endl;
    return 0;
}
