#pragma once

enum class SemanticTokenType : uint32_t
{
    NAMESPACE = 0u,
    TYPE,
    STRUCT,
    TYPE_PARAMETER,
    PARAMETER,
    VARIABLE,
    FUNCTION,
    KEYWORD,
    COMMENT,
    STRING,
    NUMBER,
    OPERATOR,
    COUNT
};
const char* g_SemanticTokenTypes[] =
{
    "namespace",
    "type",
    "struct",
    "typeParameter",
    "parameter",
    "variable",
    "function",
    "keyword",
    "comment",
    "string",
    "number",
    "operator"
};

enum class SemanticTokenModifier : uint32_t
{
    NONE = 0u,
    DECLARATION = 1 << 0,
    DEFINITION = 1 << 1,
    STATIC = 1 << 2,
    COUNT = 3
};
const uint32_t g_SemanticTokenModfiersCount = 3;
const char* g_SemanticTokenModifiers[] =
{
    "declaration",
    "definition",
    "static"
};
