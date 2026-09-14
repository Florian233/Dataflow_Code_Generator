#pragma once

#include "IR/AST/AST.hpp"
#include <map>
#include <set>

namespace AST_Transform {

	/* transform actions into a new statement comprising guards and priorities */
	AST::Statement* guardprio_cond(
		AST::AST_Root* ast,
		std::vector<AST::Action*> actions,
		std::map<std::string, std::vector<AST::Statement*>>& additional_code,
		std::map<std::string, std::string>& action_function,
		bool add_else_break = false);

	/* Transform all actions inputpatterns to channelread statements potentially wrapped in a loop and variable definitions */
	AST::AST_Root* inputpattern_channelreadstmt(
		AST::AST_Root* ast,
		std::set<std::string>& used_symbols);

	/* Transform all actions output expressions into channelwrite statements potentially wrapped in a loop */
	AST::AST_Root* outputexpr_channelwritestmt(
		AST::AST_Root* ast,
		std::set<std::string>& used_symbols);

	/* Transform a channel read statement into a variable assignment */
	void channelreadstmt_assignment(
		AST::Action* action,
		AST::InputChannelReadStatement* read,
		std::string variable,
		std::string variable_index,
		std::string variable_sz,
		unsigned channel_size,
		std::vector<AST::Statement*>* stmt_list);

	/* Transform a channel write statement into a variable assignment */
	void channelwritestmt_assignment(
		AST::Action* action,
		AST::OutputChannelWriteStatement* write,
		std::string variable,
		std::string variable_index,
		std::string variable_sz,
		unsigned channel_size,
		std::vector<AST::Statement*>* stmt_list);

	void replace_identifiers_expr(
		AST::BaseExpression* expr,
		std::map<std::string, std::string>& replacements);

	void replace_identifiers_vardef(
		std::vector<AST::VarDefinition*>& vardefs,
		std::map<std::string, std::string>& replacements);

	void replace_identifiers_stmt(
		std::vector<AST::Statement*>& statements,
		std::map<std::string, std::string>& replacements);
}