#pragma once

#include <string>
#include <algorithm>
#include <cctype>

/* Helper functions for string manipulation that are required here and there. */

static inline void replace_all_substrings(
	std::string& orig,
	const std::string& to_replace,
	const std::string& replacement)
{
	size_t pos = 0;
	while ((pos = orig.find(to_replace, pos)) != std::string::npos) {
		orig.replace(pos, to_replace.length(), replacement);
		pos += replacement.length();
	}
}

static inline bool is_delimiter(char c) {
	if ((c == '!') || (c == '=') || (c == '<') || (c == '>')
		|| (c == '&') || (c == '|') || (c == '(') || (c == ')'))
	{
		return true;
	}
	return false;
}

static inline void replace_variables(
	std::string& orig,
	const std::string& to_replace,
	const std::string& replacement)
{
	size_t pos = 0;
	while ((pos = orig.find(to_replace, pos)) != std::string::npos) {
		bool before = false;
		if (pos == 0) {
			before = true;
		}
		else {
			before = is_delimiter(orig.at(pos - 1));
		}
		bool after = false;
		if (pos + to_replace.length() >= orig.length()) {
			after = true;
		}
		else {
			after = is_delimiter(orig.at(pos + to_replace.length()));
		}

		if (before && after) {
			orig.replace(pos, to_replace.length(), replacement);
			pos += replacement.length();
		}
		else {
			pos++;
		}
	}
}

static inline void remove_whitespaces(
	std::string& orig)
{
	orig.erase(std::remove_if(orig.begin(), orig.end(), ::isspace), orig.end());
}