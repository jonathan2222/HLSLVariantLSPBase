#pragma once

#include <unordered_map>
#include <unordered_set>

// Entry functions like a hash.
// Key is a hash.
// Header should have a equal operator to it.
template<typename Key, typename Header, typename Entry>
struct MultiMap
{
	void Add(const Key& key, Header&& header, Entry&& entry)
	{
		auto it = m_Map.find(key);
		if (it != m_Map.end())
		{
#ifdef MSLP_DEBUG
			const Header& eHeader = it->second.header;
			assert(eHeader == header && "Headers need to match!");
#endif
			auto it2 = it->second.entries.find(entry);
			if (it2 != it->second.entries.end())
				return; // Already added

			it->second.entries.insert(entry);
		}
		else
		{
			std::pair<std::unordered_map<Key, HEntry>::iterator, bool> result = m_Map.exmplace(key, header);
			if (!result.second)
				return; // Failed to insert key

			result.first->entries.insert(entry);
		}
	}

	void Remove(const Key& key, Entry&& entry)
	{
		auto it = m_Map.find(key);
		if (it != m_Map.end())
		{
			bool empty = false;
			auto it2 = it->second.entries.find(entry);
			if (it2 != it->second.entries.end())
			{
				it->second.entries.erase(it2);
				empty = true;
			}

			if (empty)
				m_Map.erase(it);
		}
	}

private:
	struct HEntry
	{
		Header header;
		std::unordered_set<Entry> entries;
		HEntry(Header&& header) : header(std::move(header)) {}
	};
	std::unordered_map<Key, HEntry> m_Map;
};