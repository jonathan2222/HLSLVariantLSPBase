#pragma once

#include <lsp/types.h>

void _SendMessage(const std::string& message, lsp::MessageType type);
#define SendMessage(msg) _SendMessage(msg, lsp::MessageType::Info)
#define SendMessageDebug(msg) _SendMessage(msg, lsp::MessageType::Log)
#define SendMessageInfo(msg) _SendMessage(msg, lsp::MessageType::Info)
#define SendMessageError(msg) _SendMessage(msg, lsp::MessageType::Error)
#define SendMessageWarning(msg) _SendMessage(msg, lsp::MessageType::Warning)

void _SendLog(const std::string& message, lsp::MessageType type);
#define SendLog(msg) _SendLog(msg, lsp::MessageType::Info)
#define SendLogDebug(msg) _SendLog(msg, lsp::MessageType::Log)
#define SendLogInfo(msg) _SendLog(msg, lsp::MessageType::Info)
#define SendLogError(msg) _SendLog(msg, lsp::MessageType::Error)
#define SendLogWarning(msg) _SendLog(msg, lsp::MessageType::Warning)
