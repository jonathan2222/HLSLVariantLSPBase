#pragma once

#include <lsp/types.h>
#include <vector>
//#include <algorithm> // std::min

struct LineTracker
{
    //struct LineData
    //{
    //    uint64_t offset;
    //    uint32_t length;
    //};

    LineTracker() {}

    LineTracker(const char* pText, size_t textLength)
    {
        Create(pText, textLength);
    }

    void Create(const char* pText, size_t textLength)
    {
        m_LineOffsets.clear();

        lsp::Range range;
        range.start = { .line = 0, .character = 0 };
        range.end = { .line = 0, .character = 0 };
        ModifyRange(pText, textLength, range);
    }

    uint64_t FetchByteOffset(uint32_t line) const
    {
        if (line == 0u)
            return 0u;

        assert(m_LineOffsets.size() > line && "Trying to fetch a byte offset from a line that does not exist!");
        return m_LineOffsets[line];
    }

    uint64_t FetchByteOffset(const lsp::Position position) const
    {
        return FetchByteOffset(position.line) + position.character;
    }

    void RemoveText(const lsp::Range& range)
    {
        // Check if there is something to remove.
        if (range.start.line == range.end.line &&
            range.start.character == range.end.character)
            return;

        const uint64_t startByteOffset = FetchByteOffset(range.start);
        const uint64_t endByteOffset = FetchByteOffset(range.end);
        const int64_t removeByteCount = (int64_t)(endByteOffset - startByteOffset);

        //     removeByteCount = 5    
        // [0] .....      [0] .....  
        // [1] |....   -> [1] ...... 
        // [2] .|......   [2]        
        // [3]                        

        //     removeByteCount = 4  
        // [0] |....|    [0] ....   
        // [1] ....   -> [1] ...... 
        // [2] ......    [2]        
        // [3]                      

        //     removeByteCount = 3     
        // [0] .....      [0] .....    
        // [1] ..|..   -> [1] ........ 
        // [2] .|......   [2]          
        // [3]                         

        uint64_t oldFileSize = m_FileSize;
#ifdef MSLP_DEBUG
        assert(m_FileSize >= removeByteCount && "Cannot remove more than what exists.");
#endif
        m_FileSize -= removeByteCount;
        if (m_FileSize == 0u)
        {
            m_LineOffsets.clear();
            return;
        }

        // Remove "e = false;\nint c = foo" from:
        // [0] int a;\n
        // [1] bool e = false;\n
        // [2] int c = foo();\n
        // [3] Print(\"Error\");\n
        // Result:
        // [0] int a;\n
        // [1] bool ();\n
        // [2] Print(\"Error\");\n

        /*Example document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     if (c)\n
            [4]     \tPrint(\"Error\");\n

            Edit (Removing "if (c)\n\t"):
            Range: {[3:0], [4:1]}
            text: ""

            Result document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     Print(\"Error\");\n
        */

        // Only the last line in the range need to be change differently compared to the lines after it.
        if (range.end.line < m_LineOffsets.size() && range.start.line != range.end.line)
        {
            int64_t lastLineOffset = (int64_t)range.end.character;
            if (range.start.character != 0u)
                lastLineOffset = 0u;
            m_LineOffsets[range.end.line] -= removeByteCount - lastLineOffset;
        }

        uint32_t linesToRemove = range.end.line - range.start.line;

        for (uint32_t line = range.end.line + 1u; line < m_LineOffsets.size(); ++line)
            m_LineOffsets[line] -= removeByteCount;

        if (linesToRemove > 0)
            m_LineOffsets.erase(m_LineOffsets.begin() + range.end.line - linesToRemove, m_LineOffsets.begin() + range.end.line);
    }

    lsp::Range AddText(const char* text, size_t textLength, const lsp::Position& position)
    {
        // Check if there is something to add.
        if (textLength == 0u)
            return lsp::Range{ .start = position, .end = position };

        lsp::Range newRange;
        newRange.start = position;
        newRange.end = newRange.start;

        if (m_FileSize == 0u)
            m_LineOffsets.push_back(0u);

        uint64_t bytesAdded = textLength;
        m_FileSize += bytesAdded;

        //     bytesAdded = 5        
        // [0] .....      [0] .....   
        // [1] |...... -> [1] |....   
        // [2]            [2] .|......
        //                [3]         

        //     bytesAdded = 4       
        // [0] |....      [0] |....| 
        // [1] ...... ->  [1] ....   
        // [2]            [2] ...... 
        //                [3]        

        //     bytesAdded = 3          
        // [0] .....        [0] .....   
        // [1] ..|...... -> [1] ..|..   
        // [2]              [2] .|......
        //                  [3]         

        const uint64_t startByteOffset = FetchByteOffset(position);
        for (uint32_t i = 0, currentLine = position.line; i < textLength; ++i)
        {
            char c = text[i];
            // Ignore r but still count \r when setting byte offset.
            // This can be seen when setting currentByteOffset, that we use 'i' which takes '\r' into account.
            if (c == '\r')
                continue;

            if (c == '\n')
            {
                currentLine++;
                // The first byte on this line is the next byte in the array which is why we have +1 here.
                uint64_t currentByteOffset = startByteOffset + i + 1;

                // If this is a line number we have not seen, we will add it.
                if (currentLine >= m_LineOffsets.size())
                    m_LineOffsets.push_back(currentByteOffset);
                else
                    m_LineOffsets[currentLine] = currentByteOffset;

                // Update new range
                newRange.end.character = 0;
                newRange.end.line = currentLine;
            }

            if (i != textLength - 1 || c != '\n')
                newRange.end.character++;
        }

        // Only the last line in the range need to be change differently compared to the lines after it.
        if (newRange.end.line < m_LineOffsets.size() && newRange.end.character != 0u &&
            position.line != newRange.end.line)
        {
            int64_t lastLineOffset = (int64_t)newRange.end.character;
            m_LineOffsets[newRange.end.line] += bytesAdded - lastLineOffset;
        }

        for (uint32_t line = newRange.end.line + 1u; line < m_LineOffsets.size(); ++line)
            m_LineOffsets[line] += bytesAdded;

        return newRange;
    }

    lsp::Range ModifyRange(const char* text, size_t textLength, const lsp::Range& range)
    {
        RemoveText(range);
        return AddText(text, textLength, range.start);
    }

    const std::vector<uint64_t>& GetOffsets() const
    {
        return m_LineOffsets;
    }

private:
	// Index of the vector is the line number and the value is the byte offset of the data where the line starts.
	std::vector<uint64_t> m_LineOffsets;
    uint64_t m_FileSize = 0u; // Size in characters.
};

#ifdef MSLP_DEBUG

namespace DebugLineTracker
{
    bool EqualTo(uint64_t a, uint64_t b)
    {
        return a == b;
    }

    void UnitTest()
    {
        /*Example document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     if (c)\n
            [4]     \tPrint(\"Error\");\n

            Edit (Removing "if (c)\n\t"):
            Range: {[3:0], [4:1]}
            text: ""

            Result document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     Print(\"Error\");\n
        */
        LineTracker lineTracker;
        const char* text =
            "int a;\n"
            "bool b;\n"
            "int c = foo();\n"
            "if (c)\n"
            "\tPrint(\"Error\");\n";
        lineTracker.Create(text, strlen(text));

        uint64_t arr[] = { 0u, 7u, 15u, 30u, 37u, 54u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr), EqualTo));

        lsp::Range editRange;
        editRange.start = { .line = 3, .character = 0 };
        editRange.end = { .line = 4, .character = 1 };
        lsp::Range newRange = lineTracker.ModifyRange("", 0u, editRange);
        assert(newRange.start.line == 3 && newRange.start.character == 0);
        assert(newRange.end.line == 3 && newRange.end.character == 0);

        uint64_t arr2[] = { 0u, 7u, 15u, 30u, 46u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr2), EqualTo));

        /*Example document:
            Line    Text
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     Print(\"Error\");\n

            Edit (Replacing "b" with "e = false"):
            Range: {[1:5], [1:6]}
            text: "e = false"

            Result document:
            Line    Text
            [0]     int a;\n
            [1]     bool e = false;\n
            [2]     int c = foo();\n
            [3]     Print(\"Error\");\n
        */

        editRange.start = { .line = 1, .character = 5 };
        editRange.end = { .line = 1, .character = 6 };
        newRange = lineTracker.ModifyRange("e = false", 9u, editRange);
        assert(newRange.start.line == 1 && newRange.start.character == 5);
        assert(newRange.end.line == 1 && newRange.end.character == 14);

        uint64_t arr3[] = { 0u, 7u, 23u, 38u, 54u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr3), EqualTo));

        // Erase
        editRange.start = { .line = 0, .character = 0 };
        editRange.end = { .line = 4, .character = 0 };
        newRange = lineTracker.ModifyRange("", 0u, editRange);
        assert(newRange.start.line == 0 && newRange.start.character == 0);
        assert(newRange.end.line == 0 && newRange.end.character == 0);

        assert(lineTracker.GetOffsets().empty());

        // Undo
        const char* text2 =
            "int a;\n"
            "bool e = false;\n"
            "int c = foo();\n"
            "Print(\"Error\");\n";
        editRange.start = { .line = 0, .character = 0 };
        editRange.end = { .line = 0, .character = 0 };
        newRange = lineTracker.ModifyRange(text2, strlen(text2), editRange);
        assert(newRange.start.line == 0 && newRange.start.character == 0);
        assert(newRange.end.line == 4 && newRange.end.character == 0);

        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr3), EqualTo));

        // Replace from:
        // [0] int a;\n
        // [1] bool e = false;\n
        // [2] int c = foo();\n
        // [3] Print(\"Error\");\n

        // Remove "e = false;\nint c = foo":
        // [0] int a;\n
        // [1] bool ();\n
        // [2] Print(\"Error\");\n
        editRange.start = { .line = 1, .character = 5 };
        editRange.end = { .line = 2, .character = 11 };
        lineTracker.RemoveText(editRange);

        uint64_t arr4[] = { 0u, 7u, 16u, 32u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr4), EqualTo));

        // Add "b;\nfloat c = Ko":
        // [0] int a;\n
        // [1] bool b;\n
        // [2] float c = Ko();\n
        // [3] Print(\"Error\");\n
        const char* text3 =
            "b;\n"
            "float c = K";
        editRange.start = { .line = 1, .character = 5 };
        editRange.end = { .line = 2, .character = 11 };
        newRange = lineTracker.AddText(text3, strlen(text3), editRange.start);
        assert(newRange.start.line == 1 && newRange.start.character == 5);
        assert(newRange.end.line == 2 && newRange.end.character == 12);

        uint64_t arr5[] = { 0u, 7u, 15u, 31u, 47u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr5), EqualTo));
    }
}

#endif