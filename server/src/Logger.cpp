#include "Logger.h"

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include "Server.h"

void _SendMessage(const std::string& message, lsp::MessageType type)
{
    lsp::ShowMessageRequestParams messageParams;
    messageParams.type = type;
    messageParams.message = message;
    /*messageParams.actions = { lsp::MessageActionItem{"Title Test"} };*/
    lsp::jsonrpc::MessageId messageId = Server::pMessageHandler->messageDispatcher().sendRequest<lsp::requests::Window_ShowMessageRequest>(
        lsp::requests::Window_ShowMessageRequest::Params{ messageParams },
        [](lsp::requests::Window_ShowMessageRequest::Result&& /*result*/) {},
        [](const lsp::Error& /*error*/) {});
}

void _SendLog(const std::string& message, lsp::MessageType type)
{
    lsp::LogMessageParams messageParams;
    messageParams.type = type;
    messageParams.message = message;
    Server::pMessageHandler->messageDispatcher().sendNotification<lsp::notifications::Window_LogMessage>(lsp::notifications::Window_LogMessage::Params{ messageParams });
}
