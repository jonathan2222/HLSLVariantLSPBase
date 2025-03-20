#include "MSParser.h"

#include "TreeSitterData.h"
#include "DataHolder.h"

#include "Logger.h"

#include <format>

void MSParser::Parse(lsp::FileURI uri, const char* pText, size_t textLength, const lsp::Range* pRange)
{
    TreeSitterData& data = FetchTreeSitterData(uri);

    if (data.pParser != nullptr && pRange == nullptr)
        ts_parser_reset(data.pParser);

    // An edit occured.
    if (pRange != nullptr && data.pTree != nullptr)
    {
        TSInputEdit inputEdit;
        inputEdit.start_point = { .row = pRange->start.line, .column = pRange->start.character };
        inputEdit.old_end_point = { .row = pRange->end.line, .column = pRange->end.character };
        inputEdit.start_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(pRange->start);
        inputEdit.old_end_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(pRange->end);

        lsp::Range newRange = data.pFileBuffer->ReplaceAt(pText, textLength, *pRange);
        inputEdit.new_end_point = { .row = newRange.end.line, .column = newRange.end.character };
        inputEdit.new_end_byte = (uint32_t)data.pFileBuffer->FetchByteOffset(newRange.end);
        ts_tree_edit(data.pTree, &inputEdit);
    }
    else
    {
        data.pFileBuffer->Create(pText, textLength);
    }

    const char* pFullTex = data.pFileBuffer->CopyText();
    uint32_t fullTextSize = (uint32_t)data.pFileBuffer->GetLength();
    TSTree* pNewTree = ts_parser_parse_string(data.pParser, data.pTree, pFullTex, fullTextSize);
    if (data.pTree)
        ts_tree_delete(data.pTree);
    data.pTree = pNewTree;

    // TODO:
    // * Parse tree for linting, semantic tokens, and diagnostics
    //      * Compute Hash, Save hash and uri path into a cache (multimap kinda) 
    //              key: path
    //              header:
    //                  * TreeSitterData (Only one tree is needed for all path because they all are refering to the same file)
    //                  * hashF (hash of the file content)
    //              Each entry:
    //                  * hashD (hash of the active defines)
    //                  * linting results (symbol table, semantic tokens, diagnostics)
    //      * Go through the tree,
    //          * If an include is found:
    //              0. Add diagnostic about file path error etc. if any.
    //              1. Add it to the dependency map [dep] -> source (Used for updating files when some included file was edited)
    //              2. Compute hash
    //              3. Compare hash to cache
    //              4. If different, request parsing for it the same way (Meaning calling RequestParsing(...)).
    //              5. If same, but symbol table version is different, then add the symbol table from the include to our symbol table.
    //          * Add to the semantic types list.
    //          * Linting:
    //              * Preproc:
    //                  * If preproc ifdef/if, push to the stack.
    //                      * Check if active, if not add deprecation diag to the next nodes until endif/else if seen.
    //                              If inactive, avoid doing any other diags on these except maybe add symbols to a "deprecated" symbol table for navigation purposes?
    //                  * If preproc endif, pop the stack.
    //              * If object definition is seen, add to the symbol table (should include the name of symbol and location, maybe also the contents?).
    //              * If function definition is seen, add to the symbol table (name, return type, and argument types).
    //              * If function call is seen, check the symbol table if the arguments match.
    //              * If variable declaration is seen, check if the type is in the symbol table, and add variable name to the symbol table.
    //                  If already in the symbol table, then add diag for it.
    //              * If variable is seen, check if it is in the symbol table. And if the variable is used in an expression, check if it is valid.
    //              * Check for node syntax errors and add diag for them (missing ; or node is an ERROR or MISSING node etc.).
    //      * Save to cache
    //      * Tell Caller that the parsing is done by requesting parsing using the dependency map [this] -> some source.
    //          (This will parse the caller again, and depending on the amount of it have includes, might even parse it multiple times.)
    // * Request diagnostic refresh information fetched from the linting phase.
    // * Request semantic types refresh, tokens fetched from the linting phase.

    {
        WriteLocker lock(*data.pTextBufferMutex);
        if (data.pTextBuffer)
            delete[] data.pTextBuffer;
        data.pTextBuffer = data.pFileBuffer->CopyText();
    }

#ifdef MSLP_DEBUG
    std::string debugTreeStr = FetchDebugTreeStr(uri);
    SendLog(debugTreeStr);

    TSNode rootNode = ts_tree_root_node(data.pTree);
    char* sExpression = ts_node_string(rootNode);
    SendLog(sExpression);
    delete sExpression;

    std::string debugFileBufferInfo = data.pFileBuffer->DebugInspect();
    SendLog(debugFileBufferInfo);
#endif
}

void MSParser::WalkTree(TSNode node, uint32_t depth, std::string& debugOutput, const char* pField)
{
    uint32_t childCount = ts_node_child_count(node);

    TSPoint startPoint = ts_node_start_point(node);
    uint32_t startByte = ts_node_start_byte(node);
    TSPoint endPoint = ts_node_end_point(node);
    uint32_t endByte = ts_node_end_byte(node);
    for (uint32_t i = 0; i < depth; ++i)
        debugOutput += " ";
    std::string field;
    if (pField)
        field = std::format(" - {}", pField);
    debugOutput += std::format("{}{}\t[{}:{} - {}:{}] [{} - {}]\n",
        ts_node_type(node), field.c_str(),
        startPoint.row, startPoint.column, endPoint.row, endPoint.column, startByte, endByte);
    for (uint32_t i = 0; i < childCount; ++i)
    {
        const char* pChildField = ts_node_field_name_for_child(node, i);
        TSNode child = ts_node_child(node, i);
        WalkTree(child, depth + 1, debugOutput, pChildField);
    }
}

#ifdef MSLP_DEBUG
void MSParser::UnitTest()
{
    lsp::FileURI emptyUri;
    MSParser parser;

    // Test 1
    {
        const char* text =
            "int a;\r\n"
            "bool b;\r\n"
            "int c = foo();\r\n"
            "if (c)\r\n"
            "\tPrint(\"Error\");\r\n";
        parser.Parse(emptyUri, text, strlen(text));
        std::string debugRes = parser.FetchDebugTreeStr(emptyUri);

        lsp::Range editRange;
        editRange.start = { .line = 3, .character = 0 };
        editRange.end = { .line = 4, .character = 1 };
        parser.Parse(emptyUri, "", 0u, &editRange);
        debugRes = parser.FetchDebugTreeStr(emptyUri);

        TreeSitterDataUser::RemoveTreeSitterData(emptyUri);
    }

    // Test 2
    {
        const char* text =
            "float foo(float3 v)\r\n"
            "{\r\n"
            "\treturn v.x;\r\n"
            "}\r\n";
        parser.Parse(emptyUri, text, strlen(text));
        std::string debugRes = parser.FetchDebugTreeStr(emptyUri);

        lsp::Range editRange;
        editRange.start = { .line = 0, .character = 5 };
        editRange.end = { .line = 0, .character = 5 };
        parser.Parse(emptyUri, "3", 0u, &editRange);
        debugRes = parser.FetchDebugTreeStr(emptyUri);

        TreeSitterDataUser::RemoveTreeSitterData(emptyUri);
    }
}
#endif
