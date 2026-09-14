#pragma once

#include <string>
#include "AST.hpp"

namespace AST {
	void print_ast(AST::AST_Root* ast);

	void print_statement(
		AST::Statement* s,
		std::string prefix);
}