#pragma once

#include <cstdint>
#include <cstring> // memcpy
#include <cassert>

struct GapBuffer
{
public:
	using Type = unsigned char;

	GapBuffer(Type* pInitialData, size_t initialSize, size_t initialGapSize)
	{
#ifdef MSLP_DEBUG
		assert(initialSize != 0 && initialGapSize != 0 && "Cannot initialize empty arrays!");
#endif

		m_BufferSize = initialSize + initialGapSize;
		m_pBufferStart = new Type[m_BufferSize];

		m_GapSize = initialGapSize;
		m_pGapStart = m_pBufferStart;

		m_pGapScratchBuffer = new Type[m_GapSize];

		// Copy data
		const size_t leftSize = (size_t)(m_pBufferStart - m_pGapStart);
		const size_t rightSize = m_BufferSize - leftSize + m_GapSize;
		std::memcpy(m_pBufferStart, pInitialData, leftSize);
		std::memcpy(m_pBufferStart + leftSize + m_GapSize, pInitialData+ leftSize, rightSize);

#ifdef MSLP_DEBUG
		std::memset(m_pGapStart, m_sDebugByte, m_GapSize);
#endif
	}

	~GapBuffer()
	{
		if (m_pBufferStart)
		{
			delete[] m_pBufferStart;
			m_pBufferStart = nullptr;
			m_BufferSize = 0;
		}
		m_pGapStart = nullptr;
		m_GapSize = 0;

		if (m_pGapScratchBuffer)
		{
			delete[] m_pGapScratchBuffer;
			m_pGapScratchBuffer = nullptr;
			m_GapScratchBufferSize = 0;
		}
	}

	void Left(size_t steps = 1)
	{
		// At the left most position.
		if (m_pGapStart == m_pBufferStart)
			return;

		// Move gap position to the left, clamping it to the buffer start position if outside.
		Type* pNewGapStart = m_pGapStart;
		const size_t leftSize = (size_t)(m_pBufferStart - m_pGapStart);
		if (leftSize >= steps)
			pNewGapStart -= steps;
		else
			pNewGapStart = m_pBufferStart;

		// Move data
		if (steps == 1u)
		{
			*(pNewGapStart + m_GapSize) = *pNewGapStart;
#ifdef MSLP_DEBUG
			*pNewGapStart = m_sDebugByte;
#endif
		}
		else
		{
			// Need to use the scratch buffer if copy pase ranges overlap.
			if (steps > m_GapSize)
			{
				std::memcpy(m_pGapScratchBuffer, pNewGapStart, steps);
				std::memcpy(pNewGapStart + m_GapSize, m_pGapScratchBuffer, steps);
			}
			else
			{
				std::memcpy(pNewGapStart + m_GapSize, pNewGapStart, steps);
			}
#ifdef MSLP_DEBUG
			std::memset(pNewGapStart, m_sDebugByte, m_GapSize);
#endif
		}

		m_pGapStart = pNewGapStart;
	}

	void Right(size_t steps = 1)
	{
		// At the right most position.
		if ((m_pGapStart + m_GapSize) == (m_pBufferStart + m_BufferSize))
			return;

		// Move gap position to the right, clamping it to the buffer end position if outside.
		Type* pNewGapStart = m_pGapStart;
		const size_t leftSize = (size_t)(m_pBufferStart - m_pGapStart);
		const size_t rightSize = m_BufferSize - leftSize + m_GapSize;
		if (rightSize >= steps)
			pNewGapStart += steps;
		else
			pNewGapStart = m_pBufferStart + m_BufferSize - 1;

		// Move data
		if (steps == 1u)
		{
			*pNewGapStart = *(pNewGapStart + m_GapSize);
#ifdef MSLP_DEBUG
			*(pNewGapStart + m_GapSize) = m_sDebugByte;
#endif
		}
		else
		{
			// Need to use the scratch buffer if copy pase ranges overlap.
			if (steps > m_GapSize)
			{
				std::memcpy(m_pGapScratchBuffer, m_pGapStart + m_GapSize, steps);
				std::memcpy(m_pGapStart, m_pGapScratchBuffer, steps);
			}
			else
			{
				std::memcpy(m_pGapStart, m_pGapStart + m_GapSize, steps);
			}
#ifdef MSLP_DEBUG
			std::memset(m_pGapStart, m_sDebugByte, m_GapSize);
#endif
		}

		m_pGapStart = pNewGapStart;
	}

	void Insert(size_t index, Type data)
	{
#ifdef MSLP_DEBUG
		assert(index < (m_BufferSize - m_GapSize) && "Out of bounds!");
#endif

		// Don't let gap become zero.
		if (m_GapSize == 1)
			Grow();

		const size_t leftSize = (size_t)(m_pBufferStart - m_pGapStart);
		const size_t rightSize = m_BufferSize - leftSize + m_GapSize;
		const size_t gapEndIndex = leftSize + m_GapSize - 1;
		const size_t indexFromGapStart = index - leftSize;
		size_t bufferIndex = index >= leftSize ? (gapEndIndex + indexFromGapStart) : index;

		// TODO: Make this!
		//int64_t leftOffset = bufferIndex - leftSize;
		//int64_t gapOffset = bufferIndex < leftSize ? ();
		//const size_t steps = gapOffset < 0 ? -gapOffset : gapOffset;
		//
		//if (gapOffset == 1)
		//{
		//	m_pBufferStart[bufferIndex] = data;
		//	m_pGapStart++;
		//	m_GapSize--;
		//}
		//
		//if (gapOffset > 0)
		//	Right(steps);

		m_pBufferStart[bufferIndex] = data;
		m_pGapStart++;
		m_GapSize--;
	}

	void Erase(size_t index)
	{
#ifdef MSLP_DEBUG
		assert(index >= 0 && index < m_BufferSize && "Out of bounds!");
#endif

		m_pGapStart--;
		m_GapSize++;
	}

private:
	void Grow()
	{
		const size_t newSize = m_BufferSize * m_sGrowSizeFactor;
		Type* pNewBuffer = new Type[newSize];

		const size_t leftSize = (size_t)(m_pBufferStart - m_pGapStart);
		const size_t rightSize = m_BufferSize - leftSize + m_GapSize;
		const size_t newGapSize = newSize - leftSize - rightSize;

		// Update scratch buffer size
		if (newGapSize > m_GapScratchBufferSize)
		{
			if (m_pGapScratchBuffer)
				delete[] m_pGapScratchBuffer;
			m_pGapScratchBuffer = new Type[m_GapSize];
		}

		// Copy Left buffer
		std::memcpy(pNewBuffer, m_pBufferStart, leftSize);

		// Copy Right buffer
		std::memcpy(pNewBuffer + leftSize + newGapSize, m_pGapStart + m_GapSize, rightSize);

		// Update stored data pointers.
		delete[] m_pBufferStart;
		m_pBufferStart = pNewBuffer;
		m_BufferSize = newSize;

		m_pGapStart = pNewBuffer + leftSize;
		m_GapSize = newGapSize;

		// Debug (Fill gap with specific data)
#ifdef MSLP_DEBUG
		std::memset(m_pGapStart, m_sDebugByte, m_GapSize);
#endif
	}

private:
	inline static const uint32_t m_sGrowSizeFactor = 2;
#ifdef MSLP_DEBUG
	inline static constexpr unsigned char m_sDebugByte = 0xFF;
#endif

	Type* m_pBufferStart = nullptr;
	size_t m_BufferSize = 0;

	Type* m_pGapStart = nullptr;
	size_t m_GapSize = 0;

	Type* m_pGapScratchBuffer = nullptr; // Used for storing data that should be copied.
	size_t m_GapScratchBufferSize = 0;
};