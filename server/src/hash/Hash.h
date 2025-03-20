#pragma once

#include "MurmurHash3.h"

#include <span>

namespace Utils
{
	struct HashCode
	{
		uint64_t a;
		uint64_t b;
		HashCode() : a(0u), b(0u) {}
		HashCode(const HashCode& other) : a(other.a), b(other.b) {}
		HashCode(HashCode&& other) noexcept : a(std::exchange(other.a, 0)), b(std::exchange(other.b, 0)) {}
		HashCode& operator=(const HashCode& other) { a = other.a; b = other.b; return *this; }
		HashCode& operator=(HashCode&& other) noexcept { a = std::exchange(other.a, 0); b = std::exchange(other.b, 0); return *this; }
		bool operator==(const HashCode& rhs) const { return a == rhs.a && b == rhs.b; }
	};

	inline HashCode Hash(std::span<uint8_t> key, uint32_t seed)
	{
		HashCode result;
		MurmurHash3_x64_128(key.data(), key.size(), seed, &result.a);
		return result;
	}
}

template<>
struct std::hash<Utils::HashCode>
{
	std::size_t operator()(const Utils::HashCode& f) const
	{
		return std::hash<uint64_t>{}(f.a) ^ (std::hash<uint64_t>{}(f.b) << 1);
	}
};
