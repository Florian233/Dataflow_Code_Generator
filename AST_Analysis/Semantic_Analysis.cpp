#include "AST_Analysis.hpp"

#include <map>
#include <vector>
#include <string>
#include <tuple>
#include <iostream>
#include <assert.h>

using SymbolName = std::string;

enum class NameType
{
	IdentifierDecl,
	IdentifierDef,
	ConstIdentifierDef,
	Action,
	Function,
	Procedure,
	Parameter,
	Port,
	InputPatternIdentifier
};

static inline std::string nametype_string(NameType t)
{
	switch (t) {
	case NameType::IdentifierDecl:
		return "Identifier Declaration";
	case NameType::IdentifierDef:
		return "Identifier Definition";
	case NameType::ConstIdentifierDef:
		return "Constant Definition";
	case NameType::Action:
		return "Action Name";
	case NameType::Function:
		return "Function Name";
	case NameType::Procedure:
		return "Procedure Name";
	case NameType::Parameter:
		return "Parameter Name";
	case NameType::Port:
		return "Port Name";
	case NameType::InputPatternIdentifier:
		return "Input Pattern Identifier Name";
	default:
		return "Unknown";
	}
}

static void analyse_expression(
	AST::BaseExpression* expression,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount);
static void analyse_statement(
	AST::Statement* stmt,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount);

static inline bool is_builtin_function(SymbolName n)
{
	if ((n == "print") || (n == "println")) {
		return true;
	}
	return false;
}

static void analyse_generator(
	AST::Generator* generator,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = generator->identifier.name;
	if (generator->type != nullptr) {
		if (generator->type->size != nullptr) {
			analyse_expression(generator->type->size, symbols, parametercount);
		}
		if (symbols.contains(name)) {
			std::cout << "WARNING: Shadowing identifier \"" << name << "\" in line " <<
				generator->identifier.line << " character " << generator->identifier.character
				<< std::endl;
		}
	}
	else {
		if (symbols.contains(name)) {
			if (symbols[name] == NameType::ConstIdentifierDef) {
				std::cout << "ERROR: Modifying const variable \"" << name << "\" in line "
					<< generator->identifier.line << " character " << generator->identifier.character
					<< std::endl;
				exit(1);
			}
			if ((symbols[name] != NameType::IdentifierDecl) &&
				(symbols[name] != NameType::IdentifierDef))
			{
				std::cout << "ERROR: Identifier \"" << name << "\" (" << nametype_string(symbols[name])
					<< ") not applicable in line " << generator->identifier.line << " character " << generator->identifier.character
					<< std::endl;
				exit(1);
			}
		}
		else {
			std::cout << "ERROR: Using undefined variable \"" << name << "\" in line " <<
				generator->identifier.line << " character " << generator->identifier.character
				<< std::endl;
			exit(1);
		}
	}

	symbols[name] = NameType::IdentifierDef;

	analyse_expression(generator->start, symbols, parametercount);
	analyse_expression(generator->end, symbols, parametercount);
}

static void analyse_portpreview(
	AST::PortPreview* p,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (!symbols.contains(p->port)) {
		std::cout << "ERROR: Unknown port \"" << p->port << std::endl;
		exit(1);
	}
	if (symbols[p->port] != NameType::Port) {
		std::cout << "ERROR: Identifier \"" << p->port << "\" is not of type port, "
			<< "but " << nametype_string(symbols[p->port]) << std::endl;
		exit(1);
	}
	if (p->index != nullptr) {
		analyse_expression(p->index->index, symbols, parametercount);
	}
}

static void analyse_portsize(
	AST::PortSize* p,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (!symbols.contains(p->port)) {
		std::cout << "ERROR: Unknown port \"" << p->port << std::endl;
		exit(1);
	}
	if (symbols[p->port] != NameType::Port) {
		std::cout << "ERROR: Identifier \"" << p->port << "\" is not of type port, "
			<< "but " << nametype_string(symbols[p->port]) << std::endl;
		exit(1);
	}
}

static void analyse_portfree(
	AST::PortFree* p,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (!symbols.contains(p->port)) {
		std::cout << "ERROR: Unknown port \"" << p->port << std::endl;
		exit(1);
	}
	if (symbols[p->port] != NameType::Port) {
		std::cout << "ERROR: Identifier \"" << p->port << "\" is not of type port, "
			<< "but " << nametype_string(symbols[p->port]) << std::endl;
		exit(1);
	}
}

static void analyse_expression(
	AST::BaseExpression* expression,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (dynamic_cast<AST::Expression*>(expression) != nullptr) {
		analyse_expression(dynamic_cast<AST::Expression*>(expression)->child, symbols, parametercount);
	}
	else if (dynamic_cast<AST::Operator*>(expression) != nullptr) {
		analyse_expression(dynamic_cast<AST::Operator*>(expression)->left, symbols, parametercount);
		analyse_expression(dynamic_cast<AST::Operator*>(expression)->right, symbols, parametercount);
	}
	else if (dynamic_cast<AST::TernaryOperator*>(expression) != nullptr) {
		analyse_expression(dynamic_cast<AST::TernaryOperator*>(expression)->cond, symbols, parametercount);
		analyse_expression(dynamic_cast<AST::TernaryOperator*>(expression)->ifblock, symbols, parametercount);
		analyse_expression(dynamic_cast<AST::TernaryOperator*>(expression)->elseblock, symbols, parametercount);
	}
	else if (dynamic_cast<AST::Identifier*>(expression) != nullptr) {
		AST::Identifier* i = dynamic_cast<AST::Identifier*>(expression);
		if (!symbols.contains(i->identifier)) {
			std::cout << "ERROR: Unknown identifier \"" << i->identifier << "\" in line "
				<< i->line << " starting at character: " << i->character << std::endl;
			exit(1);
		}
		if (i->call != nullptr) {
			if ((!is_builtin_function(i->identifier) && !symbols.contains(i->identifier)) || (symbols[i->identifier] != NameType::Function)) {
				std::cout << "ERROR: Called function \"" << i->identifier << "\" line: " << i->line
					<< ", character: " << i->character << " is either not defined or not of type function." << std::endl;
				exit(1);
			}
			if (!is_builtin_function(i->identifier) && (i->call->parameters.size() != parametercount[i->identifier])) {
				std::cout << "ERROR: Parameter count doesn't match definition: Line " << i->line << " character "
					<< i->character << std::endl;
				exit(1);
			}
			for (auto e : i->call->parameters) {
				analyse_expression(e, symbols, parametercount);
			}
		}
		else {
			if (symbols[i->identifier] == NameType::IdentifierDecl) {
				std::cout << "WARNING: Using uninitialized variable \"" << i->identifier << "\" in line "
					<< i->line << " starting at character " << i->character << std::endl;
			} else if ((symbols[i->identifier] != NameType::ConstIdentifierDef) &&
				(symbols[i->identifier] != NameType::IdentifierDef) &&
				(symbols[i->identifier] != NameType::InputPatternIdentifier) &&
				(symbols[i->identifier] != NameType::Parameter))
			{
				std::cout << "ERROR: Identifier \"" << i->identifier << "\" (" << nametype_string(symbols[i->identifier])
					<< ") not applicable in line " << i->line << " character " << i->character << std::endl;
				exit(1);
			}
			for (auto x : i->indices) {
				analyse_expression(x->index, symbols, parametercount);
			}
		}
	}
	else if (dynamic_cast<AST::ListComprehension*>(expression) != nullptr) {
		auto l = dynamic_cast<AST::ListComprehension*>(expression);

		std::map<SymbolName, NameType> local_symbols = symbols;
		for (auto gen : l->generators) {
			analyse_generator(gen, local_symbols, parametercount);
		}
		for (auto expr : l->expressions) {
			analyse_expression(expr, local_symbols, parametercount);
		}
	}
	else if (dynamic_cast<AST::PortPreview*>(expression) != nullptr) {
		auto p = dynamic_cast<AST::PortPreview*>(expression);
		analyse_portpreview(p, symbols, parametercount);
	}
	else if (dynamic_cast<AST::PortFree*>(expression) != nullptr) {
		auto p = dynamic_cast<AST::PortFree*>(expression);
		analyse_portfree(p, symbols, parametercount);
	}
	else if (dynamic_cast<AST::PortSize*>(expression) != nullptr) {
		auto p = dynamic_cast<AST::PortSize*>(expression);
		analyse_portsize(p, symbols, parametercount);
	}
	else {
		/* only a literal, no check needed */
		assert(dynamic_cast<AST::Literal*>(expression) != nullptr);
	}

}

static std::pair<SymbolName, NameType> analyse_variabledefinition(
	AST::VarDefinition* vardef,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = vardef->name.name;
	NameType type;

	if (symbols.contains(name)) {
		std::cout << "WARNING: Variable \"" << name << "\" (line: " << vardef->name.line << ", character: "
			<< vardef->name.character << ") shadows " << nametype_string(symbols[name]) << std::endl;
	}

	if (vardef->type.size != nullptr) {
		analyse_expression(vardef->type.size, symbols, parametercount);
	}

	for (auto x : vardef->arrays) {
		if (vardef->type.listtype != nullptr) {
			std::cout << "ERROR: List and array are mutually exclusive! (line: " << vardef->name.line << ", character: "
				<< vardef->name.character << ")" << std::endl;
			exit(1);
		}
		analyse_expression(x, symbols, parametercount);
	}

	if (vardef->assign != nullptr) {
		if (vardef->constassign) {
			type = NameType::ConstIdentifierDef;
		}
		else {
			type = NameType::IdentifierDef;
		}

		analyse_expression(vardef->assign, symbols, parametercount);
	}
	else {
		type = NameType::IdentifierDecl;
	}

	return std::make_pair(name, type);
}

static void analyse_assignmentstatement(
	AST::AssignmentStatement* stmt,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = stmt->identifier.name;
	if (!symbols.contains(name)) {
		std::cout << "ERROR: Unknown identifier \"" << name << "\" in line "
			<< stmt->identifier.line << " starting at character: " << stmt->identifier.character
			<< std::endl;
		exit(1);
	}
	if (symbols[name] == NameType::ConstIdentifierDef) {
		std::cout << "ERROR: Modifying const variable \"" << name << "\" in line "
			<< stmt->identifier.line << " character " << stmt->identifier.character
			<< std::endl;
		exit(1);
	}
	if ((symbols[name] != NameType::IdentifierDecl) &&
		(symbols[name] != NameType::IdentifierDef))
	{
		std::cout << "ERROR: Identifier \"" << name << "\" (" << nametype_string(symbols[name])
			<< ") not applicable in line " << stmt->identifier.line << " character " << stmt->identifier.character
			<< std::endl;
		exit(1);
	}
	for (auto x : stmt->indices) {
		analyse_expression(x->index, symbols, parametercount);
	}
	analyse_expression(stmt->asgnvalue, symbols, parametercount);

	NameType t;
	if (stmt->constasgn) {
		t = NameType::ConstIdentifierDef;
	}
	else {
		t = NameType::IdentifierDef;
	}
	symbols[name] = t;
}

static void analyse_scope(
	AST::BlockStatement *block,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	for (auto var : block->vars) {
		analyse_variabledefinition(var, symbols, parametercount);
	}
	for (auto stmt : block->statements) {
		analyse_statement(stmt, symbols, parametercount);
	}
}

static void analyse_if(
	AST::IfStatement *ifstmt,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	analyse_expression(ifstmt->condition, symbols, parametercount);
	for (auto stmt : ifstmt->ifblock) {
		analyse_statement(stmt, symbols, parametercount);
	}
	if (ifstmt->elseblock != nullptr) {
		for (auto stmt : ifstmt->elseblock->statements) {
			analyse_statement(stmt, symbols, parametercount);
		}
	}
}

static void analyse_while(
	AST::WhileStatement *whilestmt,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	analyse_expression(whilestmt->condition, symbols, parametercount);
	for (auto var : whilestmt->vars) {
		analyse_variabledefinition(var, symbols, parametercount);
	}
	for (auto stmt : whilestmt->statements) {
		analyse_statement(stmt, symbols, parametercount);
	}
}

static void analyse_foreach(
	AST::ForeachStatement *foreach,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	for (auto gen : foreach->generators) {
		analyse_generator(gen, symbols, parametercount);
	}
	for (auto var : foreach->vars) {
		analyse_variabledefinition(var, symbols, parametercount);
	}
	for (auto stmt : foreach->statements) {
		analyse_statement(stmt, symbols, parametercount);
	}
}

static void analyse_callstmt(
	AST::CallStatement* call,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = call->name.name;
	if (!is_builtin_function(name) && !symbols.contains(name)) {
		std::cout << "ERROR: Unknown identifier \"" << name << "\" in line "
			<< call->name.line << " starting at character: " << call->name.character
			<< std::endl;
		exit(1);
	}
	if ((symbols[name] != NameType::Procedure) &&
		(symbols[name] != NameType::Function))
	{
		std::cout << "ERROR: Identifier \"" << name << "\" (" << nametype_string(symbols[name])
			<< ") not applicable in line " << call->name.line << " character " << call->name.character
			<< std::endl;
		exit(1);
	}
	if (!is_builtin_function(name) && (call->parameters.size() != parametercount[name])) {
		std::cout << "ERROR: Parameter count doesn't match definition: Line " << call->name.line << "character "
			<< call->name.character << std::endl;
		exit(1);
	}
	for (auto p : call->parameters) {
		analyse_expression(p, symbols, parametercount);
	}
}

static void analyse_cwritestmt(
	AST::OutputChannelWriteStatement* cwrite,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (!symbols.contains(cwrite->port.name)) {
		std::cout << "ERROR: Unknown port \"" << cwrite->port.name << "\" in line "
			<< cwrite->port.line << " starting at character: " << cwrite->port.character
			<< std::endl;
		exit(1);
	}
	if (symbols[cwrite->port.name] != NameType::Port) {
		std::cout << "ERROR: Identifier \"" << cwrite->port.name << "\" is not of type port, "
			<< "but " << nametype_string(symbols[cwrite->port.name]) << std::endl;
		exit(1);
	}
	analyse_expression(cwrite->expr, symbols, parametercount);
}

static void analyse_creadstmt(
	AST::InputChannelReadStatement* cread,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (!symbols.contains(cread->port.name)) {
		std::cout << "ERROR: Unknown port \"" << cread->port.name << "\" in line "
			<< cread->port.line << " starting at character: " << cread->port.character
			<< std::endl;
		exit(1);
	}
	if (symbols[cread->port.name] != NameType::Port) {
		std::cout << "ERROR: Identifier \"" << cread->port.name << "\" is not of type port, "
			<< "but " << nametype_string(symbols[cread->port.name]) << std::endl;
		exit(1);
	}
	if (!symbols.contains(cread->identifier.name)) {
		std::cout << "ERROR: Unknown identifier \"" << cread->identifier.name << "\" in line "
			<< cread->identifier.line << " starting at character: " << cread->identifier.character
			<< std::endl;
		exit(1);
	}
	if ((symbols[cread->identifier.name] != NameType::IdentifierDef) &&
		(symbols[cread->identifier.name] != NameType::IdentifierDecl))
	{
		std::cout << "ERROR: Port value cannot be assigned to identifier \"" << cread->identifier.name << "\""
			<< " in line " << cread->identifier.line << " character " << cread->identifier.character
			<< std::endl;
		exit(1);
	}
	if (cread->index != nullptr) {
		analyse_expression(cread->index->index, symbols, parametercount);
	}
}

static void analyse_statement(
	AST::Statement* stmt,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	if (dynamic_cast<AST::ForeachStatement*>(stmt) != nullptr) {
		analyse_foreach(dynamic_cast<AST::ForeachStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::WhileStatement*>(stmt) != nullptr) {
		analyse_while(dynamic_cast<AST::WhileStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::AssignmentStatement*>(stmt) != nullptr) {
		analyse_assignmentstatement(dynamic_cast<AST::AssignmentStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::BlockStatement*>(stmt) != nullptr) {
		analyse_scope(dynamic_cast<AST::BlockStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::CallStatement*>(stmt) != nullptr) {
		analyse_callstmt(dynamic_cast<AST::CallStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::IfStatement*>(stmt) != nullptr) {
		analyse_if(dynamic_cast<AST::IfStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::OutputChannelWriteStatement*>(stmt) != nullptr) {
		analyse_cwritestmt(dynamic_cast<AST::OutputChannelWriteStatement*>(stmt), symbols, parametercount);
	}
	else if (dynamic_cast<AST::InputChannelReadStatement*>(stmt) != nullptr) {
		analyse_creadstmt(dynamic_cast<AST::InputChannelReadStatement*>(stmt), symbols, parametercount);
	}
	else {
		assert(0);
	}
}

static bool actions_match(
	SymbolName name1,
	SymbolName name2)
{
	if (name1.empty() || name2.empty()) {
		return false;
	}
	if (name1 == name2) {
		return true;
	}
	if (name1.size() > name2.size()) {
		std::string sub = name1.substr(0, name2.size());
		if (sub == name2) {
			return true;
		}
	}
	if (name2.size() > name1.size()) {
		std::string sub = name2.substr(0, name1.size());
		if (sub == name1) {
			return true;
		}
	}

	return false;
}

/* returns true if the given name is the name of an action or action group */
static bool is_action(
	SymbolName name,
	std::map<SymbolName, NameType>& symbols,
	std::vector<std::string>& actual_actions)
{
	bool ret = false;
	for (auto x : symbols) {
		if (x.second == NameType::Action) {
			if (actions_match(x.first, name)) {
				actual_actions.push_back(x.first);
				ret = true;
			}
		}
	}
	return ret;
}

static void analyse_prios(
	std::vector<AST::Action_Priority*>& priorities,
	std::map<SymbolName, NameType> symbols)
{
	std::map<SymbolName, std::vector<SymbolName>> lower_prio_actions;
	for (auto s : symbols) {
		if (s.second == NameType::Action) {
			lower_prio_actions[s.first] = std::vector<SymbolName>();
		}
	}

	for (auto prio : priorities) {
		std::vector<SymbolName> handled;

		for (auto it = prio->prio_rel.rbegin(); it != prio->prio_rel.rend(); ++it) {
			std::vector<SymbolName> names;
			for (auto a : lower_prio_actions) {
				if (actions_match(a.first, it->name)) {
					names.push_back(a.first);
				}
			}
			if (names.empty()) {
				std::cout << "ERROR: Cannot find corresponding actions to priorities entry \""
					<< it->name << "\" in line " << it->line << " character " << it->character
					<< std::endl;
				exit(1);
			}

			for (auto a : names) {
				for (auto lower : handled) {
					if (std::find(lower_prio_actions[lower].begin(),
						lower_prio_actions[lower].end(), a) != lower_prio_actions[lower].end())
					{
						std::cout << "ERROR: Actions " << a << " and " << lower << " have a cyclic"
							<< " priority relation." << std::endl;
						exit(1);
					}
					lower_prio_actions[a].push_back(lower);
				}
			}
			for (auto a : names) {
				handled.push_back(a);
			}
		}
	}
}

static void analyse_fsm(
	AST::Schedule_FSM *fsm,
	std::map<SymbolName, NameType> symbols)
{
	std::map<SymbolName, unsigned> states;

	states[fsm->inital_state.name] = 1;

	for (auto t : fsm->transitions) {
		states[t->state.name] = 2;

		for (auto a : t->actions) {
			std::vector<std::string> actual_actions;
			if (!is_action(a.name, symbols, actual_actions)) {
				std::cout << "ERROR: FSM Definition uses undefined action \"" << a.name << "\" line: "
					<< a.line << ", character: " << a.character << std::endl;
				exit(1);
			}

			for (auto x : actual_actions) {
				t->actual_actions.push_back(std::make_pair(a.name, x));
			}

			for (auto x : fsm->transitions) {
				if ((x->state.name == t->state.name) && (x->next.name != t->next.name)) {
					for (auto y : x->actions) {
						if (actions_match(a.name, y.name)) {
							std::cout << "ERROR: Action \"" << a.name << "\" can cause different"
								<< " transitions in state " << t->state.name << std::endl;
							exit(1);
						}
					}
				}
			}
		}
	}

	for (auto t : fsm->transitions) {
		if (!states.contains(t->next.name)) {
			std::cout << "ERROR: State \"" << t->next.name << "\" is not defined." << std::endl;
			exit(1);
		}
	}
}

static SymbolName analyse_parameter(
	AST::Parameter* parameter,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = parameter->name.name;
	if (symbols.contains(name)) {
		if (symbols[name] == NameType::Parameter) {
			std::cout << "ERROR: Multiple parameters with name \"" << name << "\"; Line: " << parameter->name.line
				<< " Character: " << parameter->name.character << std::endl;
			exit(1);
		}
		else {
			std::cout << "WARNING: Parameter \"" << name << "\" (Line: " << parameter->name.line
				<< ", Character: " << parameter->name.character << ") shadows "
				<< nametype_string(symbols[name]) << std::endl;
		}
	}
	if (parameter->type.size != nullptr) {
		analyse_expression(parameter->type.size, symbols, parametercount);
	}
	return name;
}

static SymbolName analyse_function(
	AST::Function *function,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = function->name.name;

	if (symbols.contains(name)) {
		std::cout << "ERROR: Function name \"" << name << "\" colides with "
			<< nametype_string(symbols[name]) << std::endl;
		exit(1);
	}

	for (auto p : function->parameters) {
		SymbolName pname = analyse_parameter(p, symbols, parametercount);
		symbols[pname] = NameType::Parameter;
	}

	if (function->ret_type.size != nullptr) {
		analyse_expression(function->ret_type.size, symbols, parametercount);
	}

	analyse_expression(function->expression, symbols, parametercount);

	return name;
}

static SymbolName analyse_procedure(
	AST::Procedure *procedure,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = procedure->name.name;

	if (symbols.contains(name)) {
		std::cout << "ERROR: Procedure name \"" << name << "\" colides with "
			<< nametype_string(symbols[name]) << std::endl;
		exit(1);
	}
	for (auto p : procedure->parameters) {
		SymbolName pname = analyse_parameter(p, symbols, parametercount);
		symbols[pname] = NameType::Parameter;
	}
	for (auto var : procedure->vars) {
		auto def = analyse_variabledefinition(var, symbols, parametercount);
		symbols[def.first] = def.second;
	}
	for (auto stmt : procedure->statements) {
		analyse_statement(stmt, symbols, parametercount);
	}
	return name;
}

static void analyse_repeat(
	AST::Expression *expr,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	analyse_expression(expr, symbols, parametercount);
}

static SymbolName analyse_action(
	AST::Action *action,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = action->name.name;
	for (auto in : action->input_patterns) {
		SymbolName port = in->port.name;
		if (!symbols.contains(port) || (symbols[port] != NameType::Port)) {
			std::cout << "ERROR: Action \"" << name << "\" accesses non-existing port \"" << port << "\"" << std::endl;
			exit(1);
		}
		for (auto id : in->IDs) {
			if (symbols.contains(id.name)) {
				if (symbols[id.name] == NameType::InputPatternIdentifier) {
					std::cout << "ERROR: Action \"" << name << "\" has multiple Identifiers in InputPatterns with "
						<< "name \"" << id.name << "\"" << std::endl;
					exit(1);
				}
				else {
					std::cout << "WARNING: Action \"" << name << "\" input pattern of port \"" << port << "\""
						<< " identifier: \"" << id.name << "\" shadows " << nametype_string(symbols[id.name]) << std::endl;
				}
			}
			symbols[id.name] = NameType::InputPatternIdentifier;
		}
		if (in->repeat != nullptr) {
			analyse_repeat(in->repeat, symbols, parametercount);
		}
	}
	for (auto guard : action->guards) {
		analyse_expression(guard, symbols, parametercount);
	}
	for (auto var : action->vars) {
		auto def = analyse_variabledefinition(var, symbols, parametercount);
		symbols[def.first] = def.second;
	}
	for (auto stmt : action->statements) {
		analyse_statement(stmt, symbols, parametercount);
	}
	for (auto out : action->output_expressions) {
		SymbolName port = out->port.name;
		if (!symbols.contains(port) || (symbols[port] != NameType::Port)) {
			std::cout << "ERROR: Action \"" << name << "\" accesses non-existing port \"" << port << "\"" << std::endl;
			exit(1);
		}
		for (auto expr : out->expressions) {
			analyse_expression(expr, symbols, parametercount);
		}
		if (out->repeat != nullptr) {
			analyse_repeat(out->repeat, symbols, parametercount);
		}
	}
	return name;
}

static SymbolName analyse_nativefunction(
	AST::NativeFunction* native,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = native->name.name;
	if (symbols.contains(name)) {
		std::cout << "ERROR: Native function name \"" << name << "\" is already in use." << std::endl;
		exit(1);
	}
	for (auto param : native->parameters) {
		analyse_parameter(param, symbols, parametercount);
	}
	/* not checking parameters since this is only a declaration. */
	return name;
}


static SymbolName analyse_nativeprocedure(
	AST::NativeProcedure* native,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = native->name.name;
	if (symbols.contains(name)) {
		std::cout << "ERROR: Native procedure name \"" << name << "\" is already in use." << std::endl;
		exit(1);
	}
	for (auto param : native->parameters) {
		analyse_parameter(param, symbols, parametercount);
	}
	/* not checking parameters since this is only a declaration. */
	return name;
}


static SymbolName analyse_actorparam(
	AST::ActorParameter* parameter,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = parameter->name.name;
	if (symbols.contains(name)) {
		std::cout << "ERROR: Actor parameter name \"" << name << "\" is already in use." << std::endl;
		exit(1);
	}

	if (parameter->type.size != nullptr) {
		analyse_expression(parameter->type.size, symbols, parametercount);
	}

	return name;
}

static SymbolName analyse_port(
	AST::Port* port,
	std::map< SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	SymbolName name = port->name.name;
	if (symbols.contains(name)) {
		std::cout << "ERROR: Port name \"" << name << "\" is already in use." << std::endl;
		exit(1);
	}
	if (port->type.size != nullptr) {
		analyse_expression(port->type.size, symbols, parametercount);
	}
	return name;
}

static void analyse_actor(
	AST::Actor *actor,
	std::map<SymbolName, NameType> symbols,
	std::map<SymbolName, unsigned> parametercount)
{
	for (auto p : actor->parameters) {
		SymbolName name = analyse_actorparam(p, symbols, parametercount);
		symbols[name] = NameType::Parameter;
	}

	for (auto i : actor->inports) {
		SymbolName name = analyse_port(i, symbols, parametercount);
		symbols[name] = NameType::Port;
	}

	for (auto o : actor->outports) {
		SymbolName name = analyse_port(o, symbols, parametercount);
		symbols[name] = NameType::Port;
	}
	for (auto n : actor->nativefunctions) {
		SymbolName name = analyse_nativefunction(n, symbols, parametercount);
		symbols[name] = NameType::Function;
		parametercount[name] = static_cast<unsigned>(n->parameters.size());
	}
	for (auto n : actor->nativeprocedures) {
		SymbolName name = analyse_nativeprocedure(n, symbols, parametercount);
		symbols[name] = NameType::Function;
		parametercount[name] = static_cast<unsigned>(n->parameters.size());
	}
	for (auto v : actor->vars) {
		auto def = analyse_variabledefinition(v, symbols, parametercount);
		symbols[def.first] = def.second;
	}
	for (auto p : actor->procedures) {
		SymbolName name = analyse_procedure(p, symbols, parametercount);
		symbols[name] = NameType::Procedure;
		parametercount[name] = static_cast<unsigned>(p->parameters.size());
	}
	for (auto f : actor->functions) {
		SymbolName name = analyse_function(f, symbols, parametercount);
		symbols[name] = NameType::Function;
		parametercount[name] = static_cast<unsigned>(f->parameters.size());
	}
	for (auto a : actor->actions) {
		SymbolName name = analyse_action(a, symbols, parametercount);
		if (!name.empty()) {
			symbols[name] = NameType::Action;
		}
	}
	if (actor->init != nullptr) {
		(void)analyse_action(actor->init, symbols, parametercount);
	}
	if (actor->fsm != nullptr) {
		analyse_fsm(actor->fsm, symbols);
	}
	analyse_prios(actor->prios, symbols);
}

static void analyse_unit(
	AST::Unit *unit,
	std::map<SymbolName, NameType>& symbols,
	std::map<SymbolName, unsigned>& parametercount)
{
	for (auto v : unit->vars) {
		auto symbol = analyse_variabledefinition(v, symbols, parametercount);
		symbols[symbol.first] = symbol.second;
	}
	for (auto p : unit->procedures) {
		SymbolName name = analyse_procedure(p, symbols, parametercount);
		symbols[name] = NameType::Procedure;
		parametercount[name] = static_cast<unsigned>(p->parameters.size());
	}
	for (auto n : unit->nativefunctions) {
		SymbolName name = analyse_nativefunction(n, symbols, parametercount);
		symbols[name] = NameType::Function;
		parametercount[name] = static_cast<unsigned>(n->parameters.size());
	}
	for (auto f : unit->functions) {
		SymbolName name = analyse_function(f, symbols, parametercount);
		symbols[name] = NameType::Function;
		parametercount[name] = static_cast<unsigned>(f->parameters.size());
	}
}

void AST_Analysis::semantic(
	std::vector<AST::Unit*> units,
	AST::AST_Root *ast)
{
	std::map<SymbolName, NameType> symbols;
	symbols["print"] = NameType::Function;
	symbols["println"] = NameType::Function;
	std::map<SymbolName, unsigned> parametercount;

	for (auto unit : units) {
		analyse_unit(unit, symbols, parametercount);
	}

	if (ast->actor != nullptr) {
		analyse_actor(ast->actor, symbols, parametercount);
	}
	if (ast->unit != nullptr) {
		analyse_unit(ast->unit, symbols, parametercount);
	}
}