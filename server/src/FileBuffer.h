#pragma once

#include "GapBuffer.h"
#include "LineTracker.h"

#include <format>

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

    char* CopyText() const
    {
        char* pData = m_GapBuffer.CopyData(1u);
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
        const uint64_t startByteOffset = m_LineTracker.FetchByteOffset(fromRange.start);
        const uint64_t endByteOffset = m_LineTracker.FetchByteOffset(fromRange.end);

        // Modify m_LinePoionter to reflect the edit.
        lsp::Range newRange = m_LineTracker.ModifyRange(pText, textLength, fromRange);

        // First erase
        m_GapBuffer.Erase(startByteOffset, endByteOffset - startByteOffset);

        // Then add
        m_GapBuffer.Insert(startByteOffset, pText, textLength);

        return newRange;
    }

#ifdef MSLP_DEBUG
    std::string DebugInspect()
    {
        std::string result;
        std::string scratch;
        m_GapBuffer.InspectBuffersAsStrings(result, scratch);

        std::string lineTrackerInfo = std::format("LineTracker:\n\tFileSize: {}\n\tOffsets:\n", m_LineTracker.GetFileSize());
        const std::vector<uint64_t>& offsets = m_LineTracker.GetOffsets();
        for (uint32_t i = 0u; i < offsets.size(); ++i)
        {
            uint64_t lineOffset = offsets[i];
            lineTrackerInfo += std::format("\t[{}] Offset: {}\n", i, lineOffset);
        }

        return std::format("GapBuffer:\n'{}'\n{}", result.c_str(), lineTrackerInfo.c_str());
    }
#endif

private:
    inline static const size_t InitialGapCount = 256;

    GapBuffer<char> m_GapBuffer;
    LineTracker m_LineTracker;
};
