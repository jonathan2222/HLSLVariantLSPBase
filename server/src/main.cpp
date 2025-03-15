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

struct FileBuffer : public GapBuffer<char>
{
    char* CopyData(bool nullTerminated) const
    {
        char* pData = GapBuffer<char>::CopyData(nullTerminated ? 1u : 0u);
        if (nullTerminated)
            pData[GetDataCount()] = '\0';
        return pData;
    }

    uint32_t FetchByteOffsetFromLine(uint32_t line) const
    {
        assert(m_LinePointer.size() > line && "Trying to fetch a byte offset from a line that does not exist!");
        return m_LinePointer[line];
    }

    uint32_t FetchByteOffsetFromPosition(const lsp::Position position) const
    {
        return FetchByteOffsetFromLine(position.line) + position.character;
    }

    void ModifyLineData(const std::string& text, const lsp::Range& range)
    {
        uint32_t startByteOffset = FetchByteOffsetFromPosition(range.start);
        uint32_t endByteOffset = FetchByteOffsetFromPosition(range.end);
        int64_t oldBytesRange = (int64_t)(endByteOffset - startByteOffset);
        int64_t newBytesRange = (int64_t)text.size();

        uint32_t currentLine = range.start.line;

        std::vector<uint32_t> oldRangeLineData;

        uint32_t newLineRange = 0;
        // Lines in this needs to be updated.
        for (uint64_t i = 0; i < text.size(); ++i)
        {
            char c = text[i];
            if (c == '\n')
            {
                newLineRange++;

                currentLine++;
                // The first byte on this line is the next byte in the array.
                uint32_t currentByteOffset = startByteOffset + i + 1;
                if (currentLine >= m_LinePointer.size())
                    m_LinePointer.push_back(currentByteOffset);
                else
                {
                    oldRangeLineData.push_back(m_LinePointer[currentLine]);
                    m_LinePointer[currentLine] = currentByteOffset;
                }
            }
        }

        // If positive: bytes were added, negative: bytes were removed.
        int64_t bytesAdded = newBytesRange - oldBytesRange;

        // TODO: Fix this!
        uint32_t oldLineRange = range.end.line - range.start.line;
        if (oldLineRange > newLineRange)
        {
            uint32_t lineDiff = oldLineRange - newLineRange;
            m_LinePointer.resize(m_LinePointer.size() - lineDiff);
            uint32_t lineCarry = 0;
            for (uint32_t line = currentLine + 1; line < m_LinePointer.size(); ++line)
            {
                if (line - range.start.line - lineDiff < 0)
                {
                    lineCarry = oldRangeLineData[line - range.start.line - lineDiff];
                    m_LinePointer[line] = (uint32_t)(lineCarry + bytesAdded);
                }

                lineCarry = (int64_t)m_LinePointer[line];
                m_LinePointer[line] = (uint32_t)(lineCarry + bytesAdded);
            }
        }

        for (uint32_t line = currentLine + 1; line < m_LinePointer.size(); ++line)
        {
            int64_t byteOffset = (int64_t)m_LinePointer[line];
            m_LinePointer[line] = (uint32_t)(byteOffset + bytesAdded);
        }
    }

    void ReplaceAt(const std::string& text, const lsp::Range& range)
    {
        uint32_t startByteOffset = FetchByteOffsetFromPosition(range.start);
        uint32_t endByteOffset = FetchByteOffsetFromPosition(range.end);
        int64_t oldBytesRange = (int64_t)(endByteOffset - startByteOffset);
        int64_t newBytesRange = (int64_t)text.size();

        // Modify m_LinePoionter to reflect the edit.
        {
            
        }

        // First erase
        Erase(startByteOffset, endByteOffset - startByteOffset);

        // Then add
        Insert(startByteOffset, text.c_str(), text.size());
    }

private:
    std::vector<uint32_t> m_LinePointer; // Index of the vector is the line number and the value is the byte offset of the data where the line starts.
};

struct Walker
{
private:
    TSParser* m_pParser = nullptr;
    TSTree* m_pCurrentTree = nullptr;
    FileBuffer m_FileBuffer;

    uint32_t FetchByteOffsetFromPosition(const lsp::Position& position)
    {
        // What to do here?

        return 0u;
    }

public:
    Walker() { InitTreeSitter(); }
    ~Walker() { DeleteTreeSitter(); }

    // text: The text to replace the text in the range.
    // (optional) range: The range of the document that got changed
    void Parse(const std::string& text, const lsp::Range* pRange = nullptr)
    {
        if (m_pParser != nullptr && pRange == nullptr)
            ts_parser_reset(m_pParser);

        // An edit occured.
        if (pRange != nullptr)
        {
            // A row of 0 here means no change in row. If row == 0 then character is an offset of the start.
            // But if row != 0 the character is the new character position.
            lsp::Position newPositionOffset = Utils::FetchPositionFromText(text);

            TSInputEdit inputEdit;
            inputEdit.start_point = { .row = pRange->start.line, .column = pRange->start.character };
            inputEdit.old_end_point = { .row = pRange->end.line, .column = pRange->end.character };
            inputEdit.start_byte = FetchByteOffsetFromPosition(pRange->start);
            inputEdit.old_end_byte = FetchByteOffsetFromPosition(pRange->end);
            inputEdit.new_end_point =
            {
                .row = newPositionOffset.line,
                .column = newPositionOffset.line == 0 ? newPositionOffset.character : inputEdit.start_point.column + newPositionOffset.character
            };
            inputEdit.new_end_byte = inputEdit.old_end_byte + text.size();
            ts_tree_edit(m_pCurrentTree, &inputEdit);
        }

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
    std::string bufferStr, scratchStr;

#ifdef MSLP_DEBUG
    DebugGapBuffer::UnitTest();
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
