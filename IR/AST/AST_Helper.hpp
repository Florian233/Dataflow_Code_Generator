#pragma once

#include "AST.hpp"
#include <vector>

namespace AST {

	bool check_cycle(AST::Statement* s);

	bool check_cycle_list(std::vector<AST::Statement*> s);

}