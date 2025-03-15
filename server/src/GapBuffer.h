#pragma once

#include <cstdint>
#include <cstring> // memcpy
#include <cassert>

template<typename Type>
struct GapBuffer
{
public:
	GapBuffer() {}

	GapBuffer(const Type* pInitialData, size_t initialCount, size_t initialGapCount, size_t initialGapIndex = 0u)
	{
		Init(pInitialData, initialCount, initialGapCount, initialGapIndex);
	}

	virtual ~GapBuffer()
	{
		Delete();
	}

	void Create(const Type* pInitialData, size_t initialCount, size_t initialGapCount, size_t initialGapIndex = 0u)
	{
		Init(pInitialData, initialCount, initialGapCount, initialGapIndex);
	}

	void Left(size_t steps = 1u)
	{
#ifdef MSLP_DEBUG
		assert(IsInitialized());
#endif

		// At the left most position.
		// | - - - - - | 0 1 2 3 4 5 6 7 8 9
		if (m_pGapStart == m_pBufferStart)
			return;

		// Move gap position to the left, clamping it to the buffer start position if outside.
		Type* pNewGapStart = m_pGapStart;
		const size_t leftCount = CalcLeftCount();
		if (leftCount > steps)
			pNewGapStart -= steps;
		else
		{
			pNewGapStart = m_pBufferStart;
			steps = leftCount;
		}

		// Move data
		if (steps == 1u)
		{
			*(pNewGapStart + m_GapCount) = *pNewGapStart;
#ifdef MSLP_DEBUG
			*pNewGapStart = m_sDebugByte;
#endif
		}
		else
		{
			// Need to use the scratch buffer if copy paste ranges overlap.
			if (steps > m_GapCount)
			{
				GrowScratchBuffer(steps);
				std::memcpy(m_pGapScratchBuffer, pNewGapStart, sizeof(Type) * steps);
				std::memcpy(pNewGapStart + m_GapCount, m_pGapScratchBuffer, sizeof(Type) * steps);
			}
			else
			{
				std::memcpy(pNewGapStart + m_GapCount, pNewGapStart, sizeof(Type) * steps);
			}
#ifdef MSLP_DEBUG
			std::memset(pNewGapStart, m_sDebugByte, sizeof(Type) * m_GapCount);
#endif
		}

		m_pGapStart = pNewGapStart;
	}

	void Right(size_t steps = 1u)
	{
#ifdef MSLP_DEBUG
		assert(IsInitialized());
#endif

		// At the right most position.
		// 0 1 2 3 4 5 6 7 8 9 | - - - - - |
		if (m_pGapStart == (m_pBufferStart + m_BufferCount - 1u - m_GapCount - 1u))
			return;

		// Move gap position to the right, clamping it to the buffer end position if outside.
		Type* pNewGapStart = m_pGapStart;
		const size_t rightCount = CalcRightCount();
		if (rightCount > steps)
			pNewGapStart += steps;
		else
		{
			pNewGapStart = m_pBufferStart + m_BufferCount - 1;
			steps = rightCount;
		}

		// Move data
		if (steps == 1u)
		{
			*pNewGapStart = *(pNewGapStart + m_GapCount);
#ifdef MSLP_DEBUG
			*(pNewGapStart + m_GapCount) = m_sDebugByte;
#endif
		}
		else
		{
			// Need to use the scratch buffer if copy pase ranges overlap.
			if (steps > m_GapCount)
			{
				GrowScratchBuffer(steps);
				std::memcpy(m_pGapScratchBuffer, m_pGapStart + m_GapCount, sizeof(Type) * steps);
				std::memcpy(m_pGapStart, m_pGapScratchBuffer, sizeof(Type) * steps);
			}
			else
			{
				std::memcpy(m_pGapStart, m_pGapStart + m_GapCount, sizeof(Type) * steps);
			}
#ifdef MSLP_DEBUG
			std::memset(m_pGapStart + steps, m_sDebugByte, sizeof(Type) * m_GapCount);
#endif
		}

		m_pGapStart = pNewGapStart;
	}

	// Note: This moves the gap towards the index.
	// index: The index before it will be inserted. The element at that position will be on the right side of this index after insertion.
	void Insert(size_t index, const Type* pData, size_t dataCount)
	{
#ifdef MSLP_DEBUG
		assert(IsInitialized());
#endif

		// Don't let gap become zero.
		if (m_GapCount < dataCount + 1u)
			Grow();

		MoveTo(index);

		if (dataCount > 1)
			std::memcpy(m_pGapStart, pData, sizeof(Type) * dataCount);
		else
			*m_pGapStart = *pData;

		m_pGapStart += dataCount;
		m_GapCount -= dataCount;
	}

	// Note: This moves the gap towards the index.
	// index: The index before it will be inserted. The element at that position will be on the right side of this index after insertion.
	void Insert(size_t index, Type data)
	{
		Insert(index, &data, 1u);
	}

	void Erase(size_t index, size_t count = 1u)
	{
#ifdef MSLP_DEBUG
		assert(IsInitialized());
#endif
		MoveTo(index);
		GrowOverlap(count);
	}

#ifdef MSLP_DEBUG
	void InspectBuffersAsStrings(std::string& bufferStrOut, std::string& scratchStrOut) const
	{
		assert(m_BufferCount != 0u);
		assert(m_GapCount != 0u);
		assert(m_GapCapacity != 0u);

		bufferStrOut.resize(m_BufferCount);
		const size_t leftCount = CalcLeftCount();
		const size_t rightCount = CalcRightCount(leftCount);
		if (leftCount > 0)
			std::memcpy(bufferStrOut.data(), m_pBufferStart, leftCount);
		if (rightCount > 0)
			std::memcpy(bufferStrOut.data() + leftCount + m_GapCount, m_pBufferStart + leftCount + m_GapCount, rightCount);
		std::memcpy(bufferStrOut.data() + leftCount, m_pBufferStart + leftCount, m_GapCount);

		scratchStrOut.resize(m_GapCapacity);
		std::memcpy(scratchStrOut.data(), m_pGapScratchBuffer, m_GapCapacity);
	}
#endif

	size_t GetDataCount() const
	{
		const size_t leftCount = CalcLeftCount();
		const size_t rightCount = CalcRightCount(leftCount);
		return leftCount + rightCount;
	}

	// This allocates a new array. Remember to delete it when done using it!
	Type* CopyData(uint32_t extraSpace = 0u) const
	{
		const size_t leftCount = CalcLeftCount();
		const size_t rightCount = CalcRightCount(leftCount);
		const size_t count = leftCount + rightCount;

#ifdef MSLP_DEBUG
		assert(IsInitialized());
		assert(count > 0);
#endif
		Type* pData = new Type[count + extraSpace];
		if (leftCount != 0)
			std::memcpy(pData, m_pBufferStart, sizeof(Type) * leftCount);
		if (rightCount != 0)
			std::memcpy(pData + leftCount, m_pBufferStart + leftCount + m_GapCount, sizeof(Type) * rightCount);
		return pData;
	}

	bool IsEmpty() const
	{
		return !IsInitialized() || GetDataCount() == 0u;
	}

protected:
	// Moves gap such that the start of the gap is where the index pointed to.
	// Index does not include the gap:
	// 0 1 2 | - - - - | 3 4 5 
	void MoveTo(size_t index)
	{
#ifdef MSLP_DEBUG
		assert(index <= (m_BufferCount - m_GapCount) && "Out of bounds!");
#endif

		const size_t leftCount = CalcLeftCount();
		const bool onLeftSide = index < leftCount;

		// BufferStart         BufferEnd
		// |                       |
		// 0 1 2 | - - - - | 3 4 5 
		//         ^         ^
		//     GapStart    GapEnd

		// Index
		// 0 1 2 | - - - - | 3 4 5 
		// BufferIndex
		// 0 1 2 | 3 4 5 6 | 7 8 9 

		// Move the gap such that 'BufferIndex' is at the start of the gap:
		//    BufferIndex          
		//         |               
		// 0 1 2 | - - - - | 3 4 5 

		if (onLeftSide)
		{
			const size_t leftOffset = leftCount - index; // GapStart -> BufferIndex
			Left(leftOffset);
		}
		else
		{
			const size_t rightOffset = index - leftCount; // GapEnd -> BufferIndex
			Right(rightOffset);
		}
	}

	// Grows the gap without disrupting the data.
	// (Expensive due to it needing to copy all 'left' and 'right' data over to new buffer!)
	void Grow()
	{
		// 0 1 2 | - - | 3 4 5 
		// After Grow (grow factor of 2):
		// 0 1 2 | - - - - - - - - - - | 3 4 5 

		const size_t newSize = m_BufferCount * m_sGrowSizeFactor;
		Type* pNewBuffer = new Type[newSize];

		const size_t leftSize = (size_t)(m_pGapStart - m_pBufferStart);
		const size_t rightSize = m_BufferCount - leftSize - m_GapCount;
		const size_t newGapSize = newSize - leftSize - rightSize;

		// Update scratch buffer size
		GrowScratchBuffer(newGapSize);

		// Copy Left buffer
		std::memcpy(pNewBuffer, m_pBufferStart, sizeof(Type) * leftSize);

		// Copy Right buffer
		std::memcpy(pNewBuffer + leftSize + newGapSize, m_pGapStart + m_GapCount, sizeof(Type) * rightSize);

		// Update stored data pointers.
		delete[] m_pBufferStart;
		m_pBufferStart = pNewBuffer;
		m_BufferCount = newSize;

		m_pGapStart = pNewBuffer + leftSize;
		m_GapCount = newGapSize;

		// Debug (Fill gap with specific data)
#ifdef MSLP_DEBUG
		std::memset(m_pGapStart, m_sDebugByte, sizeof(Type) * m_GapCount);
#endif
	}

	// Grows the gap without copying data. This will 'erase' some data that was after the gap.
	void GrowOverlap(size_t extraCount)
	{
		// 0 1 2 | - - - - | 3 4 5 
		// ExtraCount = 2:
		// 0 1 2 | - - - - 3 4 | 5 

#ifdef MSLP_DEBUG
		assert(extraCount <= (m_BufferCount - m_GapCount) && "Cannot erase more elements than the total amount!");
		std::memset(m_pGapStart + m_GapCount, m_sDebugByte, sizeof(Type) * extraCount);
#endif
		m_GapCount += extraCount;

		GrowScratchBuffer(m_GapCount);
	}

	void Init(const Type* pInitialData, size_t initialCount, size_t initialGapCount, size_t initialGapIndex)
	{
		// Delete old data
		if (IsInitialized())
			Delete();

#ifdef MSLP_DEBUG
		assert(initialGapCount != 0 && "Cannot initialize an empty gap!");
		assert((pInitialData == nullptr && initialCount == 0) || (pInitialData != nullptr && initialCount > 0) && "pInitialData need to match the initialCount!");
		assert(initialGapIndex <= initialCount && "Gap index out of bounds!");
#endif

		m_BufferCount = initialCount + initialGapCount;
		m_pBufferStart = new Type[m_BufferCount];

		m_GapCount = initialGapCount;
		m_pGapStart = m_pBufferStart + initialGapIndex;

		m_pGapScratchBuffer = new Type[m_GapCount];
		m_GapCapacity = m_GapCount;

		// Copy data
		if (initialCount > 0 && pInitialData)
		{
			const size_t leftCount = CalcLeftCount();
			const size_t rightCount = CalcRightCount(leftCount);
			if (leftCount != 0u)
				std::memcpy(m_pBufferStart, pInitialData, sizeof(Type) * leftCount);
			if (rightCount != 0u)
				std::memcpy(m_pBufferStart + leftCount + m_GapCount, pInitialData + leftCount, sizeof(Type) * rightCount);
		}

#ifdef MSLP_DEBUG
		std::memset(m_pGapStart, m_sDebugByte, sizeof(Type) * m_GapCount);
#endif
	}

	void Delete()
	{
		if (m_pBufferStart)
		{
			delete[] m_pBufferStart;
			m_pBufferStart = nullptr;
			m_BufferCount = 0;
		}
		m_pGapStart = nullptr;
		m_GapCount = 0;

		if (m_pGapScratchBuffer)
		{
			delete[] m_pGapScratchBuffer;
			m_pGapScratchBuffer = nullptr;
			m_GapCapacity = 0;
		}
	}

	void GrowScratchBuffer(size_t newCount)
	{
		if (newCount > m_GapCapacity)
		{
			if (m_pGapScratchBuffer)
				delete[] m_pGapScratchBuffer;
			m_pGapScratchBuffer = new Type[newCount];
			m_GapCapacity = newCount;
		}
	}

	size_t CalcLeftCount() const { return (size_t)(m_pGapStart - m_pBufferStart); }
	size_t CalcRightCount(size_t leftCount) const { return m_BufferCount - leftCount - m_GapCount; }
	size_t CalcRightCount() const { return m_BufferCount - CalcLeftCount() - m_GapCount; }

	bool IsInitialized() const { return m_pBufferStart != nullptr; }

protected:
	inline static const uint32_t m_sGrowSizeFactor = 2;
#ifdef MSLP_DEBUG
	inline static constexpr uint8_t m_sDebugByte = 45u; // '-'
#endif

	Type* m_pBufferStart = nullptr;
	size_t m_BufferCount = 0;

	Type* m_pGapStart = nullptr;
	size_t m_GapCount = 0;

	Type* m_pGapScratchBuffer = nullptr; // Used for storing data that should be copied.
	size_t m_GapCapacity = 0;
};

#ifdef MSLP_DEBUG

namespace DebugGapBuffer
{
	void UnitTest()
	{
		std::string bufferStr, scratchStr;
		GapBuffer<char> gapBuffer;
		std::string str = "This is a test string with even more data in it. For this is a great block of text.";
		gapBuffer.Create(str.c_str(), str.size(), 10, 12); // Note: Add +1 to the size if you want to include the null terminator in the internal buffer.
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is a te----------st string with even more data in it. For this is a great block of text.");
		gapBuffer.Left();
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is a t----------est string with even more data in it. For this is a great block of text.");
		gapBuffer.Left(4);
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is---------- a test string with even more data in it. For this is a great block of text.");
		gapBuffer.Right(20);
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is a test string with ----------even more data in it. For this is a great block of text.");
		gapBuffer.Insert(9, " large", 6);
		// str = "This is a large test string with even more data in it. For this is a great block of text.";
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is a large---- test string with even more data in it. For this is a great block of text.");
		gapBuffer.Erase(27, 26);
		// str = "This is a large test string. For this is a great block of text.";
		gapBuffer.InspectBuffersAsStrings(bufferStr, scratchStr);
		assert(bufferStr == "This is a large test string------------------------------. For this is a great block of text.");

		char* pData = gapBuffer.CopyData(1u);
		pData[gapBuffer.GetDataCount()] = '\0';
		assert(strcmp(pData, "This is a large test string. For this is a great block of text.") == 0);
	}
}

#endif