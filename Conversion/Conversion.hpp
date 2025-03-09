#pragma once

#include <string>
#include <map>
#include "Tokenizer/Tokenizer.hpp"

namespace Conversion_Helper {

	std::string read_type(
		Token& t,
		Token_Container& token_producer,
		std::map<std::string, std::string>& symbol_map);

	int evaluate_constant_expression(
		std::string expression,
		std::map<std::string, std::string>& symbol_map);

	void read_constants(
		Token_Container& token_producer,
		std::map<std::string, std::string>& symbol_map,
		std::string symbol);

	/* Return true if the token producer contains the tokens for the declaration/definition of the requested symbol.
	 * The requested symbol can be *, in this case it anyhow returns true.
	 */
	bool symbol_name_match(
		Token_Container& token_producer,
		std::string symbol);
}