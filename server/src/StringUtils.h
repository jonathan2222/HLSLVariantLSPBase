#pragma once

#include <lsp/types.h>

#include <string>

namespace Utils
{
	// A line of 0 here means no change in line. If line == 0 then character is an offset of the start.
	// But if line != 0 the character is the new character position.
	lsp::Position FetchPositionFromText(const std::string& text)
	{
		lsp::Position position = { .line = 0, .character = (lsp::uint)(-1)};
		for (char c : text)
		{
			position.character++;
			if (c == '\n')
			{
				position.line++;
				position.character = 0;
			}
		}
	}
}