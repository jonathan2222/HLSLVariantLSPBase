#pragma once

#include "GapBuffer.h"
#include "LineTracker.h"

struct FileBuffer
{
    FileBuffer() {}

    FileBuffer(const char* pInitialData, size_t initialCount)
        : m_GapBuffer(pInitialData, initialCount, InitialGapCount, 0)
        , m_LineTracker(pInitialData, initialCount)
    {
    }

    void Create(const char* pInitialData, size_t initialCount)
    {
        m_GapBuffer.Create(pInitialData, initialCount, InitialGapCount, 0);
        m_LineTracker.Create(pInitialData, initialCount);
    }

    char* CopyText(bool nullTerminated) const
    {
        char* pData = m_GapBuffer.CopyData(nullTerminated ? 1u : 0u);
        if (nullTerminated)
            pData[m_GapBuffer.GetDataCount()] = '\0';
        return pData;
    }

    size_t GetLength() const
    {
        return m_GapBuffer.GetDataCount();
    }

    uint64_t FetchByteOffset(uint32_t line) const
    {
        return m_LineTracker.FetchByteOffset(line);
    }

    uint64_t FetchByteOffset(const lsp::Position position) const
    {
        return m_LineTracker.FetchByteOffset(position);
    }

    lsp::Range ReplaceAt(const char* pText, size_t textLength, const lsp::Range& fromRange)
    {
        const uint32_t startByteOffset = m_LineTracker.FetchByteOffset(fromRange.start);
        const uint32_t endByteOffset = m_LineTracker.FetchByteOffset(fromRange.end);

        // Modify m_LinePoionter to reflect the edit.
        lsp::Range newRange = m_LineTracker.ModifyRange(pText, textLength, fromRange);

        // First erase
        m_GapBuffer.Erase(startByteOffset, endByteOffset - startByteOffset);

        // Then add
        m_GapBuffer.Insert(startByteOffset, pText, textLength);

        return newRange;
    }

private:
    inline static const size_t InitialGapCount = 256;

    GapBuffer<char> m_GapBuffer;
    LineTracker m_LineTracker;
};
