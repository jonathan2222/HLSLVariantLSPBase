#pragma once

#include <lsp/types.h>
#include <vector>

struct LineTracker
{
    LineTracker() {}

    LineTracker(const char* pText, size_t textLength)
    {
        Create(pText, textLength);
    }

    void Clear()
    {
        m_LineOffsets.clear();
        m_FileSize = 0u;
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
        const uint64_t removeByteCount = endByteOffset - startByteOffset;

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

        //     linesToRemove   = 1   
        //     removeByteCount = 5   
        // [0] .....      [0] .....  
        // [1] |....   -> [1] ...... 
        // [2] .|......   [2]        
        // [3]                       

        //     linesToRemove   = 0  
        //     removeByteCount = 4  
        // [0] |....|    [0] ....   
        // [1] ....   -> [1] ...... 
        // [2] ......    [2]        
        // [3]                      

        //     linesToRemove   = 1     
        //     removeByteCount = 3     
        // [0] .....      [0] .....    
        // [1] ..|..   -> [1] ........ 
        // [2] .|......   [2]          
        // [3]                         

        //     linesToRemove   = 2     
        //     removeByteCount = 9     
        // [0] .....      [0] .....    
        // [1] ..|..   -> [1] ........ 
        // [2] ......     [2]          
        // [3] .|......                
        // [4]                         

        uint32_t linesToRemove = range.end.line - range.start.line;

        // Change offset of all lines after this.
        if (linesToRemove == 0)
        {
            for (uint32_t line = range.end.line + 1u; line < m_LineOffsets.size(); ++line)
                m_LineOffsets[line] -= removeByteCount;
        }
        else
        {
            // Add offsets to line start+1:
            //   (A) Compensating for what is left of line end: [(end+1).offset - (end.offset + end.character)]
            uint64_t endP1 = range.end.line + 1 < m_LineOffsets.size() ? m_LineOffsets[range.end.line+1u] : oldFileSize;
            int64_t addedLineBytes = (int64_t)endP1 - (int64_t)(m_LineOffsets[range.end.line] + range.end.character);

            //   (B) Compensating for what was removed from line start: -[(start+1).offset - (start.offset + start.character)]
            int64_t removedLineBytes = (int64_t)m_LineOffsets[range.start.line+1u] - (int64_t)(m_LineOffsets[range.start.line] + range.start.character);
            
            uint64_t lineStartNextNewOffset = m_LineOffsets[range.start.line + 1] + (addedLineBytes - removedLineBytes);

            // Then we remove the lines (start, end].
            m_LineOffsets.erase(m_LineOffsets.begin() + range.start.line + 1u, m_LineOffsets.begin() + range.end.line + 1u);

            if (range.start.line + 1 < m_LineOffsets.size())
                m_LineOffsets[range.start.line + 1] = lineStartNextNewOffset;

            // After which we do the same as the case for linesToRemove == 0
            for (uint32_t line = range.start.line + 2u; line < m_LineOffsets.size(); ++line)
                m_LineOffsets[line] -= removeByteCount;
        }
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

        //     addedLines = 0              
        //     bytesAdded = 10             
        // [0] .....      [0] .....        
        // [1] |...... -> [1] |....|...... 
        // [2]            [2]              
        // 
        //     addedLines = 1         
        //     bytesAdded = 5         
        // [0] .....      [0] .....   
        // [1] |...... -> [1] |....   
        // [2]            [2] .|......
        //                [3]         

        //     addedLines = 1        
        //     bytesAdded = 4        
        // [0] |....      [0] |....| 
        // [1] ...... ->  [1] ....   
        // [2]            [2] ...... 
        //                [3]        

        //     addedLines = 1           
        //     bytesAdded = 3           
        // [0] .....        [0] .....   
        // [1] ..|...... -> [1] ..|..   
        // [2]              [2] .|......
        //                  [3]         

        //     addedLines = 2           
        //     bytesAdded = 9           
        // [0] .....        [0] .....   
        // [1] ..|...... -> [1] ..|..   
        // [2]              [2] ......  
        //                  [3] .|......
        //                  [4]         

        std::vector<uint64_t> newLines;

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
                //if (currentLine >= m_LineOffsets.size())
                //    m_LineOffsets.push_back(currentByteOffset);
                //else
                //    m_LineOffsets[currentLine] = currentByteOffset;

                newLines.push_back(currentByteOffset);

                // Update new range
                newRange.end.character = 0;
                newRange.end.line = currentLine;
            }
            else
                newRange.end.character++;
        }

        // Insert line offsets (start, new.end]
        uint32_t linesAdded = newRange.end.line - newRange.start.line;

        if (linesAdded > 0)
            m_LineOffsets.insert(m_LineOffsets.begin() + newRange.start.line + 1u, newLines.begin(), newLines.end());

        // Update offsets of all lines from end+1
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

    uint64_t GetFileSize() const
    {
        return m_FileSize;
    }

private:
	// Index of the vector is the line number and the value is the byte offset of the data where the line starts.
	std::vector<uint64_t> m_LineOffsets;
    uint64_t m_FileSize = 0u; // Size in characters.

public:
#ifdef MSLP_DEBUG

    static bool _EqualTo(uint64_t a, uint64_t b)
    {
        return a == b;
    }

    static void UnitTest()
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
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr), _EqualTo));

        lsp::Range editRange;
        editRange.start = { .line = 3, .character = 0 };
        editRange.end = { .line = 4, .character = 1 };
        lsp::Range newRange = lineTracker.ModifyRange("", 0u, editRange);
        assert(newRange.start.line == 3 && newRange.start.character == 0);
        assert(newRange.end.line == 3 && newRange.end.character == 0);

        uint64_t arr2[] = { 0u, 7u, 15u, 30u, 46u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr2), _EqualTo));

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
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr3), _EqualTo));

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

        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr3), _EqualTo));

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
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr4), _EqualTo));

        // Add "b;\nfloat c = Ko":
        // [0] int a;\n
        // [1] bool b;\n
        // [2] float c = Ko();\n
        // [3] Print(\"Error\");\n
        const char* text3 =
            "b;\n"
            "float c = Ko";
        lsp::Position position = { .line = 1, .character = 5 };
        newRange = lineTracker.AddText(text3, strlen(text3), position);
        assert(newRange.start.line == 1 && newRange.start.character == 5);
        assert(newRange.end.line == 2 && newRange.end.character == 12);

        uint64_t arr5[] = { 0u, 7u, 15u, 31u, 47u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr5), _EqualTo));

        /*
        *   flaot a;\n
        *   flaot b;\n
        *   flaot c;\n
        * 
        * Remove gives:
        *   flaot a;\n
        *   flaot b;\n
        */

        lineTracker.Clear();
        const char* text4 =
            "flaot a;\r\n"
            "flaot b;\r\n"
            "flaot c;";
        lineTracker.Create(text4, strlen(text4));

        editRange.start = { .line = 1, .character = 8 };
        editRange.end = { .line = 2, .character = 8 };
        lineTracker.RemoveText(editRange);

        uint64_t arr6[] = { 0u, 10u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr6), _EqualTo));
    }
#endif

};