#include "Server.h"

#include <format>

#include "MSParser.h"
#include "Logger.h"

#include "SemanticTokens.h"

#include "DocumentLinkProvider.h"
#include "SemanticTokensProvider.h"

lsp::Connection* Server::pConnection = nullptr;

void Server::Run()
{
    // 3: Register callbacks for incoming messages
    pMessageHandler->requestHandler()
        // Request callbacks always have the message id as the first parameter followed by the params if there are any.
        .add<lsp::requests::Initialize>(RequestInitialize)
        // Notifications don't have an id parameter because no response is sent back for them.
        .add<lsp::notifications::Exit>(RequestExit)
        .add<lsp::notifications::TextDocument_DidOpen>(NotificationDidOpen)
        .add<lsp::notifications::TextDocument_DidChange>(NotificationDidChange)
        .add<lsp::notifications::TextDocument_DidClose>(NotificationDidClose)
        .add<lsp::requests::TextDocument_Diagnostic>([](const lsp::jsonrpc::MessageId& /*id*/,
            lsp::requests::TextDocument_Diagnostic::Params&& params)
            {
                lsp::requests::TextDocument_Diagnostic::Result result;
                lsp::RelatedFullDocumentDiagnosticReport relatedFullDocumentDiagnosticReport;

                lsp::FullDocumentDiagnosticReport fullDocumentDiagnosticReport;

                lsp::Diagnostic diagnostic;
                fullDocumentDiagnosticReport.items.push_back(diagnostic);

                result = relatedFullDocumentDiagnosticReport;
                return result;
            })
        .add<lsp::requests::TextDocument_DocumentLink>(DocumentLinkProvider::Provider)
        .add<lsp::requests::TextDocument_SemanticTokens_Full>(SemanticTokensProvider::Provider);

    // 4: Start the message processing loop
    // processIncomingMessages Reads all current messages from the connection and if there are none waits until one becomes available
    try
    {
        while (m_sIsRunning)
            pMessageHandler->processIncomingMessages();
    }
    catch (lsp::ConnectionError e)
    {
        // Lost connection
        //e.what();
    }
}

Server::~Server()
{
    delete pConnection;
    pConnection = nullptr;

    delete pMessageHandler;
    pMessageHandler = nullptr;
}

lsp::requests::Initialize::Result Server::RequestInitialize(const lsp::jsonrpc::MessageId&, lsp::requests::Initialize::Params&& params)
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

    lsp::DocumentLinkOptions documentLinkOptions;
    documentLinkOptions.resolveProvider = false;
    result.capabilities.documentLinkProvider = documentLinkOptions;

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
}

void Server::NotificationDidOpen(lsp::DidOpenTextDocumentParams&& params)
{
    SendMessage(std::format("Opened TextDocument: {}", params.textDocument.uri.path().c_str()));

    const lsp::FileURI& uri = params.textDocument.uri;
    std::string& text = params.textDocument.text;
    MSParser::Parse(uri, text.c_str(), text.size());
}

void Server::NotificationDidChange(lsp::DidChangeTextDocumentParams&& params)
{
    SendMessage(std::format("Changed TextDocument: {}", params.textDocument.uri.path().c_str()));

    const lsp::FileURI& uri = params.textDocument.uri;

    for (uint32_t changeIndex = 0; changeIndex < params.contentChanges.size(); ++changeIndex)
    {
        lsp::TextDocumentContentChangeEvent& changeEvent = params.contentChanges[changeIndex];
        if (std::holds_alternative<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent))
        {
            lsp::TextDocumentContentChangeEvent_Range_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Range_Text>(changeEvent);
            MSParser::Parse(uri, textEvent.text.c_str(), textEvent.text.size(), &textEvent.range);
        }
        else
        {
            lsp::TextDocumentContentChangeEvent_Text& textEvent = std::get<lsp::TextDocumentContentChangeEvent_Text>(changeEvent);
            MSParser::Parse(uri, textEvent.text.c_str(), textEvent.text.size());
        }
    }
}

void Server::NotificationDidClose(lsp::DidCloseTextDocumentParams&& params)
{
    SendMessage(std::format("Closed TextDocument: {}", params.textDocument.uri.path().c_str()));
}
