#pragma once

#include "MurmurHash3.h"

#include <span>

namespace Utils
{
	struct HashCode
	{
		uint64_t a;
		uint64_t b;
	};

	inline HashCode Hash(std::span<uint8_t> key, uint32_t seed)
	{
		HashCode result;
		MurmurHash3_x64_128(key.data(), key.size(), seed, &result.a);
		return result;
	}
}