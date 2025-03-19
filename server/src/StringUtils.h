#pragma once

#include <lsp/types.h>
#include <string_view>
#include <span>

namespace Utils
{
	/*
	* Split the string with a specified delimiter.
	* Tip: Use a std::string_view will avoid any copy of the original string.
	*/
	template<typename T>
	inline void Split(std::vector<T>& vector, const T& str, char delimiter)
	{
#ifdef MSLP_DEBUG
		assert(vector.empty());
#endif
		if (str.empty())
			return;

		size_t prePos = 0;
		size_t pos = str.find(delimiter);
		while (pos != std::string::npos)
		{
			size_t count = pos - prePos;
			if (count != 0)
				vector.push_back(str.substr(prePos, count));
			prePos = pos + 1;
			pos = str.find(delimiter, prePos);
		}

		if (prePos < str.length())
			vector.push_back(str.substr(prePos));
	}

	/*
	* Split the string with a specified delimiter.
	*/
	inline std::vector<std::string> Split(const std::string& str, char delimiter)
	{
		std::vector<std::string> vector;
		Split<std::string>(vector, str, delimiter);
		return vector;
	}

	/*
	* Split the string_view with a specified delimiter.
	*/
	inline std::vector<std::string_view> Split(const std::string_view& str, char delimiter)
	{
		std::vector<std::string_view> vector;
		Split<std::string_view>(vector, str, delimiter);
		return vector;
	}

	inline std::string ToLower(const std::string& s)
	{
		std::string res = s;
		std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
		return res;
	}

	inline std::string ToUpper(const std::string& s)
	{
		std::string res = s;
		std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::toupper(c); });
		return res;
	}

	inline std::string ReplaceAll(const std::string& s, const std::string& from, const std::string& to)
	{
		std::string newString = s;
		if (s.empty() || (from.empty() && to.empty()))
			return newString;

		size_t pos = newString.find(from);
		while (pos != std::string::npos)
		{
			newString.replace(pos, from.size(), to);
			pos = newString.find(from, pos);
		}

		return newString;
	}

	/*
	* Check if a string ends with a certain string.
	* PS: Can be used at compile time too!
	*/
	inline constexpr bool EndsWith(std::string_view string, std::string_view ending)
	{
		const size_t stringLength = string.size();
		const size_t endingLength = ending.size();

		if (endingLength > stringLength) return false;
		if (endingLength == stringLength) return string == ending;
		if (endingLength == 0 || stringLength == 0) return false;

		// Using string view to skip allocating memory.
		const std::string_view stringView = string;
		const std::string_view stringEnding = stringView.substr(stringLength - endingLength, endingLength);

		return stringEnding == ending;
	}

	/*
	* Check if a string starts with a certain string.
	* PS: Can be used at compile time too!
	*/
	inline constexpr bool StartsWith(std::string_view string, std::string_view starting)
	{
		const size_t stringLength = string.size();
		const size_t startingLength = starting.size();

		if (startingLength > stringLength) return false;
		if (startingLength == stringLength) return string == starting;
		if (startingLength == 0 || stringLength == 0) return false;

		// Using string view to skip allocating memory.
		const std::string_view stringView = string;
		const std::string_view stringStarting = stringView.substr(0, startingLength);

		return stringStarting == starting;
	}

	// Trim from start (in place)
	inline void TrimLeft(std::string& s) {
		// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring?page=1&tab=scoredesc#tab-top
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
			}));
	}

	// Trim from end (in place)
	inline void TrimRight(std::string& s) {
		// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring?page=1&tab=scoredesc#tab-top
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
			}).base(), s.end());
	}

	// Trim from start (in place)
	inline void TrimLeft(std::string& s, char c) {
		// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring?page=1&tab=scoredesc#tab-top
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [c](unsigned char ch) {
			return ch != c;
			}));
	}

	// Trim from end (in place)
	inline void TrimRight(std::string& s, char c) {
		// From: https://stackoverflow.com/questions/216823/how-to-trim-a-stdstring?page=1&tab=scoredesc#tab-top
		s.erase(std::find_if(s.rbegin(), s.rend(), [c](unsigned char ch) {
			return ch != c;
			}).base(), s.end());
	}

	inline void Trim(std::string& str)
	{
		TrimLeft(str);
		TrimRight(str);
	}

	/*
	* Take the elements upto index (including).
	* Meaning [0 - index]
	*/
	template<typename T>
	inline std::span<T> Take(const std::span<T>& source, uint32_t index)
	{
#ifdef MSLP_DEBUG
		assert(index < source.size() && "Trying to take out of bounds!");
#endif
		return source.subspan(0, index + 1);
	}

	inline std::string JoinStrings(char c, const std::span<std::string_view>& parts)
	{
		std::string result;
		// At least one char between each part and that each part is not empty (meaning at least 1 char)
		result.reserve((parts.size()-1)*2 + 1);
		for (uint32_t i = 0u; i < parts.size(); ++i)
		{
			if (i != 0u)
				result.push_back(c);
			result += parts[i];
		}
		return result;
	}

	inline std::string GetPathFromRelativePath(std::string_view relativePath, const lsp::FileURI& referenceAbsolutePath)
	{
		std::string mainPath = ReplaceAll(referenceAbsolutePath.path(), "\\", "/");
		TrimLeft(mainPath, '/');

		std::vector<std::string_view> mainPathParts = Split(std::string_view(mainPath), '/');
		if (mainPathParts.empty())
			return "";

		for (int32_t i = (int32_t)mainPathParts.size() - 1; i >= 0; --i)
		{
			std::string currentPathBase = JoinStrings('/', Take(std::span(mainPathParts), i));
			std::string currentPath = currentPathBase + "/";
			currentPath += relativePath;

			if (!std::filesystem::path(currentPath).has_extension())
				continue;

			if (!std::filesystem::exists(currentPath))
				continue;

			return currentPath;
		}

		return "";
	}
}