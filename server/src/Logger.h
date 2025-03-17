#pragma once

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

void _SendMessage(lsp::MessageHandler& messageHandler, const std::string& message, lsp::MessageType type)
{
    lsp::ShowMessageRequestParams messageParams;
    messageParams.type = type;
    messageParams.message = message;
    /*messageParams.actions = { lsp::MessageActionItem{"Title Test"} };*/
    lsp::jsonrpc::MessageId messageId = messageHandler.messageDispatcher().sendRequest<lsp::requests::Window_ShowMessageRequest>(
        lsp::requests::Window_ShowMessageRequest::Params{ messageParams },
        [](lsp::requests::Window_ShowMessageRequest::Result&& /*result*/) {},
        [](const lsp::Error& /*error*/) {});
}
#define SendMessage(msg) _SendMessage(*g_pMessageHandler, msg, lsp::MessageType::Info)
#define SendMessageDebug(msg) _SendMessage(*g_pMessageHandler, msg, lsp::MessageType::Log)
#define SendMessageInfo(msg) _SendMessage(*g_pMessageHandler, msg, lsp::MessageType::Info)
#define SendMessageError(msg) _SendMessage(*g_pMessageHandler, msg, lsp::MessageType::Error)
#define SendMessageWarning(msg) _SendMessage(*g_pMessageHandler, msg, lsp::MessageType::Warning)

void _SendLog(lsp::MessageHandler& messageHandler, const std::string& message, lsp::MessageType type)
{
    lsp::LogMessageParams messageParams;
    messageParams.type = type;
    messageParams.message = message;
    messageHandler.messageDispatcher().sendNotification<lsp::notifications::Window_LogMessage>(lsp::notifications::Window_LogMessage::Params{ messageParams });
}
#define SendLog(msg) _SendLog(*g_pMessageHandler, msg, lsp::MessageType::Info)
#define SendLogDebug(msg) _SendLog(*g_pMessageHandler, msg, lsp::MessageType::Log)
#define SendLogInfo(msg) _SendLog(*g_pMessageHandler, msg, lsp::MessageType::Info)
#define SendLogError(msg) _SendLog(*g_pMessageHandler, msg, lsp::MessageType::Error)
#define SendLogWarning(msg) _SendLog(*g_pMessageHandler, msg, lsp::MessageType::Warning)
