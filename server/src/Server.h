#pragma once

#include <lsp/messages.h>
#include <lsp/connection.h>
#include <lsp/io/standardio.h>
#include <lsp/messagehandler.h>

struct Server
{
	// Used for all communication between server and client.
	inline static lsp::MessageHandler* pMessageHandler = nullptr;

	static void Init()
	{
		// 1: Establish a connection using standard input/output
		pConnection = new lsp::Connection{ lsp::io::standardInput(), lsp::io::standardOutput() };

		// 2: Create a MessageHandler with the connection
		pMessageHandler = new lsp::MessageHandler(*pConnection);

		m_sIsRunning = true;
	}

	static void Run();

	~Server();

private:
    static lsp::requests::Initialize::Result RequestInitialize(const lsp::jsonrpc::MessageId& id, lsp::requests::Initialize::Params&& params);
    static void RequestExit()
    {
        m_sIsRunning = false;
    }

    static void NotificationDidOpen(lsp::DidOpenTextDocumentParams&& params);
    static void NotificationDidChange(lsp::DidChangeTextDocumentParams&& params);
    static void NotificationDidClose(lsp::DidCloseTextDocumentParams&& params);

private:
	inline static bool m_sIsRunning = false;
	static lsp::Connection* pConnection;
};