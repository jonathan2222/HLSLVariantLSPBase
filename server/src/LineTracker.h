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

    lsp::Range ModifyRange(const char* text, size_t textLength, const lsp::Range& range)
    {
        lsp::Range newRange;
        newRange.start = range.start;
        newRange.end = newRange.start;

        const uint64_t startByteOffset = FetchByteOffset(range.start);
        const uint64_t endByteOffset = FetchByteOffset(range.end);
        const int64_t oldBytesRange = (int64_t)(endByteOffset - startByteOffset);
        const int64_t newBytesRange = (int64_t)textLength;

        // "if (c)\n\t"
        //  ...... . .
        //  0        7

        std::vector<uint64_t> oldRangeLineData;

        // Because we only care about the offset of the byte after the nl character, we do not need to change it on the line index of where we edited.
        // Which is why we only update the offset if we see an nl character. We do not care about what is before this because we can infer it with the old byte offset from the start of this range.

        /*

            Example document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     if (c)\n
            [4]     \tPrint("Error");\n

            Edit (Removing "if (c)\n\t"):
            Range: {[3:0], [4:1]}
            text: ""

            Result document:
            Line    Text
            [0]     int a;\n
            [1]     bool b;\n
            [2]     int c = foo();\n
            [3]     Print("Error");\n

        */

        // Counter for the lines in the 'text' variable.
        uint32_t newLineRange = 0;

        if (m_LineOffsets.empty())
        {
            newLineRange++;
            m_LineOffsets.push_back(0u); // First line have always byte offset set to zero.
        }

        // Lines in this needs to be updated.
        uint32_t validCharIndex = 0; // Ignoring '\r'
        for (uint32_t i = 0, currentLine = range.start.line; i < textLength; ++i)
        {
            char c = text[i];
            if (c == '\r') // Ignore r.
                continue;

            validCharIndex++;
            if (c == '\n')
            {
                newLineRange++;

                currentLine++;
                // The first byte on this line is the next byte in the array which is why we have +1 here.
                uint64_t currentByteOffset = startByteOffset + i + 1;

                // If this is a line number we have not seen, we will add it.
                if (currentLine >= m_LineOffsets.size())
                    m_LineOffsets.push_back(currentByteOffset);
                else
                {
                    // Else we need to save the old data such that we can update the next ones accordingly.
                    oldRangeLineData.push_back(m_LineOffsets[currentLine]);
                    m_LineOffsets[currentLine] = currentByteOffset;
                }

                // Update new range
                newRange.end.character = 0;
                newRange.end.line = currentLine;
            }

            newRange.end.character++;
        }

        // If positive: bytes were added, negative: bytes were removed.
        int64_t bytesAdded = newBytesRange - oldBytesRange;

        uint32_t oldLineRange = range.end.line - range.start.line; // Does not need to account for the current line
        if (oldLineRange > newLineRange) // Deletion of lines and Change of byte offsets
        {
            //              C     
            //  0   1   2   3   4
            // [.] [.] [.] [.] [.]
            int32_t lineDiff = newLineRange - oldLineRange;
            // Update next lines
            for (uint32_t line = range.end.line; line < m_LineOffsets.size(); ++line)
            {
                int64_t byteOffset = (int64_t)m_LineOffsets[line];
                m_LineOffsets[line] = (uint64_t)(byteOffset + bytesAdded);
            }

            // Remove old lines (meaning shift byte data)
            m_LineOffsets.erase(m_LineOffsets.begin() + range.end.line + lineDiff, m_LineOffsets.begin() + range.end.line);

        }
        else if (oldLineRange < newLineRange) // Addition of lines and Change of byte offsets
        {
            //              C     
            //  0   1   2   3   4
            // [.] [.] [.] [.] [.]
            int32_t lineDiff = newLineRange - oldLineRange;

            // Insert old lines that were pushed aside (lines after range.start.line + lineDiff)
            if (oldRangeLineData.empty() == false && oldRangeLineData.size() > lineDiff)
                m_LineOffsets.insert(m_LineOffsets.begin() + range.start.line + lineDiff,
                    oldRangeLineData.begin() + lineDiff, oldRangeLineData.end());

            // Update next lines
            for (uint32_t line = range.start.line + lineDiff; line < m_LineOffsets.size(); ++line)
            {
                int64_t byteOffset = (int64_t)m_LineOffsets[line];
                m_LineOffsets[line] = (uint64_t)(byteOffset + bytesAdded);
            }

        }
        else // Change of byte offsets
        {
            // Changes were made that affects byte offsets on all lines from range start line + 1.
            for (uint32_t line = range.start.line + 1; line < m_LineOffsets.size(); ++line)
            {
                int64_t byteOffset = (int64_t)m_LineOffsets[line];
                m_LineOffsets[line] = (uint64_t)(byteOffset + bytesAdded);
            }
        }

        return newRange;
    }

    const std::vector<uint64_t>& GetOffsets() const
    {
        return m_LineOffsets;
    }

private:
	// Index of the vector is the line number and the value is the byte offset of the data where the line starts.
	std::vector<uint64_t> m_LineOffsets;
};

#ifdef MSLP_DEBUG

namespace DebugLineTracker
{
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
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr)));

        lsp::Range editRange;
        editRange.start = { .line = 3, .character = 0 };
        editRange.end = { .line = 4, .character = 0 };
        lsp::Range newRange = lineTracker.ModifyRange("", 0u, editRange);
        assert(newRange.start.line == 3 && newRange.start.character == 0);
        assert(newRange.end.line == 3 && newRange.end.character == 0);

        uint64_t arr2[] = { 0u, 7u, 15u, 30u, 47u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr2)));

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

        uint64_t arr3[] = { 0u, 7u, 23u, 38u, 55u };
        assert(std::equal(lineTracker.GetOffsets().begin(), lineTracker.GetOffsets().end(), std::begin(arr3)));
    }
}

#endif