#include <iostream>
#include <assert.h>
#include "Parser.hpp"
using namespace Parser;

/* Error handling */
static void parser_error(
	Lexer::Token current,
	std::string expected)
{
	std::string msg;
	msg = "Expected: " + expected + " but found " + current.str + " type: ";
	switch (current.type) {
	case Lexer::Delimiter :
		msg += "Delimiter";
		break;
	case Lexer::Delimiter2:
		msg += "Delimiter2";
		break;
	case Lexer::Identifier:
		msg += "Identifier";
		break;
	case Lexer::Keyword:
		msg += "Keyword";
		break;
	case Lexer::NumericLiteral:
		msg += "NumericLiteral";
		break;
	case Lexer::Operator:
		msg += "Operator";
		break;
	case Lexer::String:
		msg += "String";
		break;
	default:
		msg += "Unknown";
		break;
	}
	msg += "\n";
	throw Parser_Exception_Unexpected_Symbol{ current.line, current.character, msg };
}

static void print_nonterm_type(NonterminalType t)
{
	constexpr std::string_view names[] =
	{
		"Start",
		"CALFile",
		"Package",
		"CALFileContent",
		"Unit",
		"UnitBody",
		"UnitEnd",
		"Actor",
		"ActorEnd",
		"ActorBody",
		"IOSig",
		"QualID",
		"QualIDList",
		"QualIDListElement",
		"ActorPars",
		"ActorPar",
		"ActorParsList",
		"PortDecl",
		"PortDecls",
		"PortDeclList",
		"Imports",
		"Import",
		/* Scheduling */
		"ActionSchedule",
		"ScheduleFSM",
		"ScheduleFSMEnd",
		"StateTransitionList",
		"StateTransition",
		"ActionTags",
		"ActionTagList",
		"PriorityOrder",
		"PriorityInequality",
		"PriorityInequalityList",
		"ActionTagInequalityList",
		/* Types */
		"Type",
		"IntegerType",
		"IntegerTypes",
		"Size",
		"ListType",
		"FloatType",
		"FloatTypes",
		/* Operations */
		"BooleanOperator",
		"ArithOperator",
		"BitOperator",
		"relOperator",
		"unaryBooleanOperator",
		"unaryBitOperator",
		"unaryArithOperator",
		"Op",
		"unaryOp",
		/* Expressions */
		"Expression",
		"Expressions",
		"ExpressionList",
		"ConstCalc",
		"SingleConstCalc",
		"SingleConstCalcList",
		"SingleExpression",
		"SingleExpressionList",
		"Negation",
		"ExpressionLiteral",
		"IfExpression",
		"IfEnd",
		"ListComprehension",
		"ListComprehensionContent",
		"ListComprehensionGenerator",
		"ListComprehensionExprs",
		"ListComprehensionExprsList",
		"Generator",
		"Generators",
		"GeneratorList",
		"CallExpr",
		"CallParameters",
		"Index",
		"SingleExpressionAdd",
		/* Functions */
		"FuncDecl",
		"FuncDeclEnd",
		"ProcDecl",
		"ProcDeclEnd",
		"NativeDecl",
		"NativeDeclDes",
		"NativeFuncDecl",
		"NativeProcedureDecl",
		"VarDecl",
		"VarDecls",
		"VarDeclList",
		"ArrayDef",
		"FormalPar",
		"FormalPars",
		"FormalParsList",
		/* Statement */
		"Statement",
		"IDStmt",
		"AssignmentStmt",
		"AssignmentExpr",
		"CallStmt",
		"BlockStmt",
		"BlockVarStmt",
		"IfStmt",
		"ElseStmt",
		"WhileStmt",
		"WhileStmtEnd",
		"ForeachStmt",
		"ForeachGenerator",
		"ForeachGeneratorList",
		"ForeachStmtEnd",
		"GeneratorType",
		"Statements",
		"ConstAsgn",
		"Asgn",
		/* Action */
		"Action",
		"InitializationAction",
		"InitializationActionEnd",
		"ActionTag",
		"ActionTagIDs",
		"ActionEnd",
		"ActionHead",
		"IDs",
		"IDList",
		"GuardBlock",
		"VarBlock",
		"ActionName",
		"StatementBlock",
		"InputPattern",
		"InputPatterns",
		"InputPatternList",
		"OutputExpression",
		"OutputExpressions",
		"OutputExpressionList",
		"OutputExpressionContent",
		"OutputExpressionContentList",
		"RepeatClause",
		"InitializerHead",
		/* Simplifications for AST Generation */
		"IndexEnd",
		"TypeEnd",
		"GuardEnd",
		"RepeatEnd",
		"CallExpressionEnd",
		"LiteralEnd",
		"UnaryOpEnd",
		"OpEnd"
	};

		std::cout << "Type: " << names[t] << std::endl;
}

/* Keller */
static void push_start(std::vector<Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CALFile });
}
static void push_CALFile(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{.type = Nonterminal, .nonterm_type= CALFileContent });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Imports });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Package });
}
static void push_Package(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = QualID });
	keller.push_back(Symbol{ .term_str = "package", .type = Terminal, .term_type = Keyword });
}
static void push_Imports(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Imports });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Import });
}
static void push_Import(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = QualID });
	keller.push_back(Symbol{ .term_str = "import", .type = Terminal, .term_type = Keyword });
}
static void push_Unit(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitEnd });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitBody });
	keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	keller.push_back(Symbol{ .term_str = "unit", .type = Terminal, .term_type = Keyword });
}
static void push_Actor(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorEnd });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionSchedule });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InitializationAction });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
	keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IOSig });
	keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorPars });
	keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	keller.push_back(Symbol{ .term_str = "actor", .type = Terminal, .term_type = Keyword });
}

static void push_IOSig(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDecls });
	keller.push_back(Symbol{ .term_str = "==>", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDecls });
}

static void push_QualID(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = QualIDList });
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
}

static void push_QualIDList(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = QualIDListElement });
	keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
}

static void push_ActorPars(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorParsList });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorPar });
}

static void push_ActorPar(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstAsgn });
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
}

static void push_ActorParsList(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorParsList });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorPar });
	keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
}

static void push_PortDecl(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
}

static void push_PortDecls(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDeclList });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDecl });
}

static void push_PortDeclList(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDeclList });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PortDecl });
	keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
}

static void push_ActionSchedule(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{.type = Nonterminal, .nonterm_type = PriorityOrder });
	keller.push_back(Symbol{.type = Nonterminal, .nonterm_type = ScheduleFSM });
}

static void push_ScheduleFSM(std::vector<Parser::Symbol>& keller)
{
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ScheduleFSMEnd });
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = StateTransitionList });
	keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
	keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	keller.push_back(Symbol{ .term_str = "fsm", .type = Terminal, .term_type = Keyword });
	keller.push_back(Symbol{ .term_str = "schedule", .type = Terminal, .term_type = Keyword });
}

/* Parse */

static inline bool is_type(Lexer::Token current)
{
	if ((current.type == Lexer::Keyword) &&
		((current.str == "list") || (current.str == "uint") || (current.str == "int") ||
			(current.str == "float") || (current.str == "half") || (current.str == "String") ||
			(current.str == "bool")))
	{
		return true;
	}
	else {
		return false;
	}

}

static inline bool is_arithop(Lexer::Token current)
{
	if ((current.type == Lexer::Operator) &&
		((current.str == "+") || (current.str == "-") || (current.str == "/") || (current.str == "*") || (current.str == "%")))
	{
		return true;
	}
	else {
		return false;
	}
}

static inline bool is_op(Lexer::Token current)
{
	if ((current.type == Lexer::Operator) &&
		((current.str =="+") || (current.str == "-") || (current.str == "/") || (current.str == "*") || (current.str == "%") || (current.str == "<") ||
		(current.str == ">") || (current.str == "=") || (current.str == "!=") || (current.str == ">=") || (current.str == "<=") ||
		(current.str == "<<") || (current.str == ">>") || (current.str == "^") || (current.str == "||") || (current.str == "&&") ||
		(current.str == "&") || (current.str == "|")))
	{
		return true;
	}
	else {
		return false;
	}
}

static inline bool is_unaryop(Lexer::Token current)
{
	if ((current.type == Lexer::Operator) &&
		((current.str =="!") || (current.str == "~") || (current.str == "++") || (current.str =="--")))
	{
		return true;
	}
	else {
		return false;
	}
}

static inline bool is_expressionliteral(Lexer::Token current)
{
	if ((current.type == Lexer::NumericLiteral) || (current.type == Lexer::String) ||
		((current.type == Lexer::Keyword) && ((current.str == "true") || (current.str == "false") || (current.str == "null"))))
	{
		return true;
	}
	else {
		return false;
	}
}

static void parse_Start(
	std::vector<Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	/* nothing to do for parse ops */

	push_start(keller);
}

static void parse_CALFile(
	std::vector<Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) &&
		((current.str == "package") ||
		 (current.str == "import") ||
		 (current.str == "actor") ||
		 (current.str == "unit")))
	{
		push_CALFile(keller);
	}
	else {
		parser_error(current, "package, import, actor, unit");
	}
}

static void parse_Package(
	std::vector<Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "package")) {
		push_Package(keller);
	}
	else {
		/* Empty nothing to do - we can check follow set here, actually it is already covered by parse_package, but let's make it complete */
		if ((current.type == Lexer::Keyword) &&
			((current.str == "import") ||
			 (current.str == "actor") ||
			 (current.str == "unit")))
		{
			/* Nothing to do, follow is okay */
		}
		else {
			parser_error(current, "import, actor, unit");
		}
	}
}

static void parse_Imports(
	std::vector<Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Import();
	if ((current.type == Lexer::Keyword) && (current.str == "import")) {
		push_Imports(keller);
	}
	else {
		if ((current.type == Lexer::Keyword) &&
			((current.str == "actor") ||
			 (current.str == "unit")))
		{
			/* Nothing to do, follow is okay */
		}
		else {
			parser_error(current, "actor, unit");
		}
	}
}

static void parse_Import(
	std::vector<Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Import();

	if ((current.type == Lexer::Keyword) && (current.str == "import")) {
		push_Import(keller);
	}
	else {
		parser_error(current, "import");
	}
}

static void parse_CALFileContent(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "actor")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Actor });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "unit")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Unit });
	}
	else {
		parser_error(current, "unit, actor");
	}
}

static void parse_Unit(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Unit();
	if ((current.type == Lexer::Keyword) && (current.str == "unit")) {
		push_Unit(keller);
	}
	else {
		parser_error(current, "unit");
	}
}

static void parse_UnitBody(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_VarDefinition(); /* there is no endsymbol for VarDecl in ActorBody, this should do as well */
	if ((current.type == Lexer::Keyword) &&
		((current.str == "list") || (current.str == "uint") || (current.str == "int") ||
			(current.str == "float") || (current.str == "half") || (current.str == "String")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitBody });
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecl });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "function")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FuncDecl });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "procedure")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ProcDecl });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "@")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnitBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = NativeDecl });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endunit"))) {
		/* okay nothing to do, follow set. */
	}
	else {
		parser_error(current, "end, endunit");
	}
}

static void parse_UnitEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Unit();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endunit")) {
		keller.push_back(Symbol{ .term_str = "endunit", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endunit");
	}
}

static void parse_Actor(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Actor();
	if ((current.type == Lexer::Keyword) && (current.str == "actor")) {
		push_Actor(keller);
	}
	else {
		parser_error(current, "actor");
	}
}

static void parse_ActorBody(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_VarDefinition(); /* there is no endsymbol for VarDecl in ActorBody, this should do as well */
	if ((current.type == Lexer::Keyword) &&
		((current.str == "list") || (current.str == "uint") || (current.str == "int") ||
			(current.str == "float") || (current.str == "half") || (current.str == "String")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecl });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "function")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FuncDecl });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "procedure")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ProcDecl });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "@")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = NativeDecl });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endactor") ||
				(current.str == "initialize") || (current.str == "schedule") || (current.str == "priority")))
	{
		/* okay nothing to do, follow set. */
	}
	else if (((current.type == Lexer::Keyword) && (current.str == "action")) ||
			(current.type == Lexer::Identifier))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActorBody });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Action });
	}
	else {
		parser_error(current, "Identifier end endactor list int uint float half bool string procedure function @ initialize schedule priority action");
	}

}

static void parse_ActorEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Actor();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endactor")) {
		keller.push_back(Symbol{ .term_str = "endactor", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endactor");
	}
}

static void parse_IOSig(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (((current.type == Lexer::Operator) && (current.str == "==>")) || is_type(current)) {
		push_IOSig(keller);
	}
	else {
		parser_error(current, "==>, Type");
	}
}

static void parse_QualID(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_IDList();
	if (current.type == Lexer::Identifier) {
		push_QualID(keller);
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_QualIDList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ".")) {
		push_QualIDList(keller);
	}
	else if ((current.type == Lexer::Delimiter2) && (current.str == ";")) {
		builder.end_IDList();
		/* Follow set, nothing to do, empty */
	}
	else {
		parser_error(current, ". ;");
	}
}

static void parse_QualIDListElement(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = QualIDList });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "*")) {
		builder.end_IDList();
		keller.push_back(Symbol{ .term_str = "*", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "Identifier *");
	}
}

static void parse_ActorPars(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_type(current)) {
		push_ActorPars(keller);
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == ")")) {
		/* Nothing to do, follow set, empty*/
	}
	else {
		parser_error(current, "type )");
	}
}

static void parse_ActorPar(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_ActorPar();
	if (is_type(current)) {
		push_ActorPar(keller);
	}
	else {
		parser_error(current, "type");
	}
}

static void parse_ActorParsList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_ActorPar();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		push_ActorParsList(keller);
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == ")")) {
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", )");
	}
}

static void parse_PortDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_PortDecl();
	if (is_type(current)) {
		push_PortDecl(keller);
	}
	else {
		parser_error(current, "type");
	}
}

static void parse_PortDecls(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_type(current)) {
		push_PortDecls(keller);
	}
	else if (((current.type == Lexer::Operator) && (current.str == "==>")) ||
		((current.type == Lexer::Delimiter) && (current.str == ":")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "type");
	}
}

static void parse_PortDeclList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_PortDecl();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		push_PortDeclList(keller);
	}
	else if (((current.type == Lexer::Operator) && (current.str == "==>")) ||
		((current.type == Lexer::Delimiter) && (current.str == ":")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ",");
	}
}

static void parse_ActionSchedule(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	push_ActionSchedule(keller);
}

static void parse_ScheduleFSM(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "schedule")) {
		builder.start_scheduleFSM();
		push_ScheduleFSM(keller);
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "priority") || (current.str == "end") || (current.str == "endactor"))) {
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "schedule, priority, end, endactor");
	}
}

static void parse_ScheduleFSMEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_scheduleFSM();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endschedule")) {
		keller.push_back(Symbol{ .term_str = "endschedule", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endschedule");
	}
}

static void parse_StateTransitionList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_StateTransition();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = StateTransitionList });
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = StateTransition });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endschedule"))) {
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Identifier end endschedule");
	}
}

static void parse_StateTransition(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_StateTransition();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = "-->", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTags });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_ActionTags(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_ActionTagList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == ")"))
	{
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", )");
	}
}

static void parse_PriorityOrder(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "priority")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PriorityInequalityList });
		keller.push_back(Symbol{ .term_str = "priority", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endactor"))) {
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "priority end endactor");
	}
}

static void parse_PriorityInequality(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_PriorityInequality();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagInequalityList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
		keller.push_back(Symbol{ .term_str = ">", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_PriorityInequalityList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_PriorityInequality();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PriorityInequalityList });
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = PriorityInequality });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		/* nothing to do, follow set, empty */

	}
	else {
		parser_error(current, "ActionTag end");
	}
}

static void parse_ActionTagInequalityList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == ">")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagInequalityList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
		keller.push_back(Symbol{ .term_str = ">", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter2) && (current.str == ";")) {
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "> ;");
	}
}

static void parse_Type(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{

	if ((current.type == Lexer::Keyword) && (current.str == "list")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListType });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "half") || (current.str == "float"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FloatType });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "uint") || (current.str == "int"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IntegerType });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "bool")) {
		builder.start_Type();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = TypeEnd });
		keller.push_back(Symbol{ .term_str = "bool", .type = Terminal, .term_type = Keyword });
	}
	else if (current.type == Lexer::String) {
		builder.start_Type();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = TypeEnd });
		keller.push_back(Symbol{ .term_str = "String", .type = Terminal, .term_type = String });
	}
	else {
		parser_error(current, "Type");
	}
}

static void parse_IntegerType(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Type();
	if ((current.type == Lexer::Keyword) && ((current.str == "uint") || (current.str == "int"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = TypeEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Size });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IntegerTypes });
	}
	else {
		parser_error(current, "uint int");
	}
}

static void parse_IntegerTypes(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "uint")) {
		keller.push_back(Symbol{ .term_str = "uint", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "int")) {
		keller.push_back(Symbol{ .term_str = "int", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "uint int");
	}
}

static void parse_Size(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstCalc });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "size", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Delimiter2) && (current.str == ",")) ||
		     ((current.type == Lexer::Delimiter) && (current.str == ":")) ||
			 (current.type == Lexer::Identifier) ||
			 ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endfunction"))))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "( Identifier");
	}
}

static void parse_ListType(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Type();
	if ((current.type == Lexer::Keyword) && (current.str == "list")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = TypeEnd });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstCalc });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "size", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IntegerType });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "type", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "list", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "List");
	}
}

static void parse_FloatType(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Type();
	if ((current.type == Lexer::Keyword) && ((current.str == "half") || (current.str == "float"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = TypeEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Size });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FloatTypes });
	}
	else {
		parser_error(current, "half float");
	}
}

static void parse_FloatTypes(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "half")) {
		keller.push_back(Symbol{ .term_str = "half", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "float")) {
		keller.push_back(Symbol{ .term_str = "float", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "half float");
	}
}

static void parse_BooleanOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Operator();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OpEnd });
	if ((current.type == Lexer::Operator) && (current.str == "||")) {
		keller.push_back(Symbol{ .term_str = "||", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "&&")) {
		keller.push_back(Symbol{ .term_str = "&&", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "||, &&");
	}
}

static void parse_ArithOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Operator();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OpEnd });
	if ((current.type == Lexer::Operator) && (current.str == "+")) {
		keller.push_back(Symbol{ .term_str = "+", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "-")) {
		keller.push_back(Symbol{ .term_str = "-", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "/")) {
		keller.push_back(Symbol{ .term_str = "/", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "*")) {
		keller.push_back(Symbol{ .term_str = "*", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "%")) {
		keller.push_back(Symbol{ .term_str = "%", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "+, -, /, *, %");
	}
}

static void parse_BitOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Operator();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OpEnd });
	if ((current.type == Lexer::Operator) && (current.str == "|")) {
		keller.push_back(Symbol{ .term_str = "|", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "&")) {
		keller.push_back(Symbol{ .term_str = "&", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "<<")) {
		keller.push_back(Symbol{ .term_str = "<<", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == ">>")) {
		keller.push_back(Symbol{ .term_str = ">>", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "^")) {
		keller.push_back(Symbol{ .term_str = "^", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "|, &, <<, >>, ^");
	}
}

static void parse_relOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Operator();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OpEnd });
	if ((current.type == Lexer::Operator) && (current.str == "<")) {
		keller.push_back(Symbol{ .term_str = "<", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == ">")) {
		keller.push_back(Symbol{ .term_str = ">", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "<=")) {
		keller.push_back(Symbol{ .term_str = "<=", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == ">=")) {
		keller.push_back(Symbol{ .term_str = ">=", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "=")) {
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "!=")) {
		keller.push_back(Symbol{ .term_str = "!=", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "<, >, <=, >=, =, !=");
	}
}

static void parse_unaryBooleanOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "!")) {
		keller.push_back(Symbol{ .term_str = "!", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "!");
	}
}

static void parse_unaryBitOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "~")) {
		keller.push_back(Symbol{ .term_str = "~", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "~");
	}
}

static void parse_unaryArithOperator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "++")) {
		keller.push_back(Symbol{ .term_str = "++", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "--")) {
		keller.push_back(Symbol{ .term_str = "--", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "++, --");
	}
}

static void parse_Op(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && ((current.str == "||") || (current.str == "&&"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = BooleanOperator });
	}
	else if ((current.type == Lexer::Operator) &&
		((current.str == "+") || (current.str == "-") || (current.str == "*") || (current.str == "/") || (current.str =="%")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ArithOperator });
	}
	else if ((current.type == Lexer::Operator) &&
		((current.str == "|") || (current.str == "&") || (current.str == "^") || (current.str == "<<") || (current.str == ">>")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = BitOperator });
	}
	else if ((current.type == Lexer::Operator) &&
		((current.str == "<") || (current.str == ">") || (current.str == "=") || (current.str == "<=") || (current.str == ">=") || (current.str == "!=")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = relOperator });
	}
	else {
		parser_error(current, "operator");
	}
}

static void parse_unaryOp(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "!")) {
		builder.start_UnaryOp();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnaryOpEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = unaryBooleanOperator });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "~")) {
		builder.start_UnaryOp();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnaryOpEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = unaryBitOperator });
	}
	else if ((current.type == Lexer::Operator) && ((current.str == "++") || (current.str == "--"))) {
		builder.start_UnaryOp();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = UnaryOpEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = unaryArithOperator });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ":") || (current.str == "(") || (current.str == "]") || (current.str == ")"))) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ";") || (current.str == ",") || (current.str == "."))) ||
		((current.type == Lexer::Keyword) && ((current.str == "do") || (current.str == "var") ||
			(current.str == "end") || (current.str == "endif") || (current.str == "endaction") ||
			(current.str == "endinitialize") || (current.str == "then") || (current.str == "else") ||
			(current.str == "endif"))) ||
		(current.type == Lexer::Identifier) || is_op(current))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "unaryoperation ( ) ] ; : , . Identifier Op do var end endaction endif endinitialize");
	}
}

static void parse_Expression(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Expression();
	if (((current.type == Lexer::Delimiter) && (current.str == "(")) ||
		is_unaryop(current) || (current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && (current.str == "if")) ||
		is_expressionliteral(current))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleExpression });
	}
	else {
		parser_error(current, "if unaryOp ( Identifier expressionLiteral");
	}
}

static void parse_Expressions(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (((current.type == Lexer::Keyword) && (current.str == "if")) ||
		((current.type == Lexer::Delimiter) && (current.str == "(")) ||
		(is_unaryop(current) || (current.type == Lexer::Identifier)) ||
		is_expressionliteral(current))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
	}

	else {
		parser_error(current, "if unaryOp ( Identifier expressionLiteral");
	}
}

static void parse_ExpressionList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ")") || (current.str == ":") || (current.str == "]"))) ||
			(current.type == Lexer::Keyword) &&
				((current.str == "end") || (current.str == "endaction") ||
				 (current.str == "endinitialize") || (current.str =="var") || (current.str == "do")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", ) : ] end endaction endinitialize var do");
	}
}

static void parse_ConstCalc(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Expression();

	if ((current.type == Lexer::NumericLiteral) ||
		((current.type == Lexer::Delimiter) && (current.str == "(")) ||
		(current.type == Lexer::Identifier))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleConstCalcList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleConstCalc });
	}
	else {
		parser_error(current, "Integer ( Identifier");
	}
}

static void parse_SingleConstCalc(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::NumericLiteral) {
		builder.start_Literal();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = LiteralEnd });
		keller.push_back(Symbol{ .term_str = "Integer", .type = Terminal, .term_type = Number });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstCalc });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	}
	else if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Integer ( Identifier");
	}
}

static void parse_SingleConstCalcList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_arithop(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleConstCalcList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleConstCalc });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ArithOperator });
	}
	else if (((current.type == Lexer::Delimiter) && (current.str == ")")) || 
		((current.type == Lexer::Operator) && (current.str == "==>")) ||
		((current.type == Lexer::Delimiter2) && (current.str == ",")) ||
		((current.type == Lexer::Keyword) && ((current.str == "guard") || (current.str == "var") || (current.str == "do") || (current.str == "end") ||
			(current.str == "endaction") || (current.str == "endinitialize"))))
	{
		builder.end_Expression();
		/* Nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "ArithOp ==> ) , endinitialize endaction end do var guard");
	}
}

static void parse_SingleExpression(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_unaryop(current) || (current.type == Lexer::Identifier))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = unaryOp });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleExpressionAdd });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = unaryOp });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "if")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IfExpression });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	}
	else if (is_expressionliteral(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ExpressionLiteral });
	}
	else {
		parser_error(current, "unaryop Identifier if [ ( literal");
	}
}

static void parse_SingleExpressionAdd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CallExpr });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Index });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ":") || (current.str == "]") || (current.str == ")"))) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ",") || (current.str == ";") || (current.str == "."))) ||
		((current.type == Lexer::Keyword) && ((current.str == "do") || (current.str == "var") || (current.str == "end") ||
			(current.str == "endaction") || (current.str == "endinitialize") ||
			(current.str == "then") || (current.str == "else") || (current.str == "endif"))) ||
		is_unaryop(current) ||
		is_op(current))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "( [ : . , ; do end enaction endinitialize op unaryop");
	}
}

static void parse_SingleExpressionList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_op(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = SingleExpression });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Op });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ":") || (current.str == "]") || (current.str == ")"))) ||
		(current.type == Lexer::Delimiter2) ||
		((current.type == Lexer::Keyword) &&
			((current.str == "do") || (current.str == "var") || (current.str == "end") ||
			 (current.str == "endaction") || (current.str == "endinitialize") ||
			 (current.str == "then") || (current.str == "else") || (current.str == "endif"))) ||
		is_unaryop(current))
	{
		builder.end_Expression();
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Op : . , ; do end endaction endinitialize unaryop then else endif ]");
	}
}

static void parse_Negation(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "-")) {
		keller.push_back(Symbol{ .term_str = "-", .type = Terminal, .term_type = Special });
	}
	else if (current.type == Lexer::NumericLiteral) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "- Number");
	}
}

static void parse_ExpressionLiteral(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Literal();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = LiteralEnd });
	if (((current.type == Lexer::Operator) && (current.str == "-")) || (current.type == Lexer::NumericLiteral)) {
		keller.push_back(Symbol{ .term_str = "Number", .type = Terminal, .term_type = Number });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Negation });
	}
	else if (current.type == Lexer::String) {
		keller.push_back(Symbol{ .term_str = "String", .type = Terminal, .term_type = String });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "true")) {
		keller.push_back(Symbol{ .term_str = "true", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "false")) {
		keller.push_back(Symbol{ .term_str = "false", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "null")) {
		keller.push_back(Symbol{ .term_str = "null", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "- Number String true false null");
	}
}

static void parse_IfExpression(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_TernaryExpression();
	if ((current.type == Lexer::Keyword) && (current.str == "if")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IfEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "else", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "then", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "if", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "if");
	}
}

static void parse_IfEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_TernaryExpression();
	builder.end_IfStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endif")) {
		keller.push_back(Symbol{ .term_str = "endif", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endif");
	}
}

static void parse_ListComprehension(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Expression();
	builder.start_ListComprehension();
	if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		keller.push_back(Symbol{ .term_str = "]", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionContent });
		keller.push_back(Symbol{ .term_str = "[", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "[");
	}
}

static void parse_ListComprehensionContent(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_unaryop(current) || is_expressionliteral(current) || (current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && (current.str == "if")) ||
		((current.type == Lexer::Delimiter) && ((current.str == "(") || (current.str == "["))))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionGenerator });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionExprs });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "]")) {
		builder.end_ListComprehension();
		builder.end_Expression();
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "unaryOp expressionLiteral ( if Identifier [ ]");
	}
}

static void parse_ListComprehensionGenerator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == ":")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Generators });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "]")) {
		builder.end_ListComprehension();
		builder.end_Expression();
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ": ]");
	}
}

static void parse_ListComprehensionExprs(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionExprsList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehension });
	}
	else if (is_unaryop(current) || is_expressionliteral(current) || (current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && (current.str == "if")) ||
		((current.type == Lexer::Delimiter) && (current.str == "(")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionExprsList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
	}
	else {
		parser_error(current, "UnaryOp Literal Identifier if ( [");
	}
}

static void parse_ListComprehensionExprsList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehensionExprs });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && ((current.str == ":") || (current.str == "]"))) {
		/* nothing to do, follow set */
	}
	else {
		parser_error(current, ", : ]");
	}
}

static void parse_Generator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Generator();
	if ((current.type == Lexer::Keyword) && (current.str == "for")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "in", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GeneratorType });
		keller.push_back(Symbol{ .term_str = "for", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "for");
	}
}

static void parse_Generators(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "for")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GeneratorList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Generator });
	}
	else {
		parser_error(current, "for");
	}
}

static void parse_GeneratorList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Generator();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GeneratorList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Generator });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str =="]")) {
		builder.end_ListComprehension();
		builder.end_Expression();
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", ]");
	}
}

static void parse_CallParameters(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == ")")) {
		/* nothing to do, follow set */
	}
	else {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expressions });
	}
}

static void parse_CallExpr(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_CallExpression();
	if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CallExpressionEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CallParameters });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "(");
	}
}

static void parse_Index(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		builder.start_Index();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Index });
		keller.push_back(Symbol{ .term_str = "]", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IndexEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "[", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ":") || (current.str == "]"))) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ",") || (current.str == ".") || (current.str == ";"))) ||
		((current.type == Lexer::Keyword) && ((current.str == "do") || (current.str == "var") || (current.str == "end") || (current.str == "endaction") || (current.str == "endinitialize"))) ||
		is_unaryop(current) ||
		is_op(current))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "[ : , . ; do end endaction endinitialize unaryOp Op");
	}
}

static void parse_FuncDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Function();
	if ((current.type == Lexer::Keyword) && (current.str == "function")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FuncDeclEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
		keller.push_back(Symbol{ .term_str = "-->", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPars });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = "function", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "function");
	}
}

static void parse_FuncDeclEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Function();
	builder.end_NativeFunction();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endfunction")) {
		keller.push_back(Symbol{ .term_str = "endfunction", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endfunction");
	}
}

static void parse_ProcDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Procedure();
	if ((current.type == Lexer::Keyword) && (current.str == "procedure")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ProcDeclEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "begin", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarBlock });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPars });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = "procedure", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "procedure");
	}
}

static void parse_ProcDeclEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Procedure();
	builder.end_NativeProcedure();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endprocedure")) {
		keller.push_back(Symbol{ .term_str = "endprocedure", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endprocedure");
	}
}

static void parse_NativeDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "@")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = NativeDeclDes });
		keller.push_back(Symbol{ .term_str = "native", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = "@", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "@native");
	}
}

static void parse_NativeDeclDes(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "function")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = NativeFuncDecl });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "procedure")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = NativeProcedureDecl });
	}
	else {
		parser_error(current, "function procedure");
	}
}

static void parse_NativeFuncDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_NativeFunction();
	if ((current.type == Lexer::Keyword) && (current.str == "function")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FuncDeclEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
		keller.push_back(Symbol{ .term_str = "-->", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPars });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = "function", .type = Terminal, .term_type = Keyword });

	}
	else {
		parser_error(current, "function");
	}
}

static void parse_NativeProcedureDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_NativeProcedure();
	if ((current.type == Lexer::Keyword) && (current.str == "procedure")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ProcDeclEnd });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPars });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = "procedure", .type = Terminal, .term_type = Keyword });

	}
	else {
		parser_error(current, "procedure");
	}
}

static void parse_VarDecl(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_VarDefinition();
	if (is_type(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Asgn });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ArrayDef });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
	}
	else {
		parser_error(current, "Type");
	}
}

static void parse_VarDecls(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_type(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDeclList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecl });
	}
	else {
		parser_error(current, "Type");
	}
}

static void parse_VarDeclList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_VarDefinition();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDeclList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecl });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "end") || (current.str == "do") || (current.str == "begin") || (current.str == "endaction") ||
			(current.str == "endinitialize")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", do begin end endaction endinitialize");
	}
}

static void parse_ArrayDef(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		builder.start_Array();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ArrayDef });
		keller.push_back(Symbol{ .term_str = "]", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstCalc });
		keller.push_back(Symbol{ .term_str = "[", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "end") || (current.str == "do") || (current.str == "begin") || (current.str == "endaction") ||
			(current.str == "endinitialize")) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ",") || (current.str == ";"))) ||
		((current.type == Lexer::Operator) && (current.str == "=")) ||
		((current.type == Lexer::Delimiter) && (current.str == ":")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "[ : = , ; do begin end endaction endinitialize");
	}
}

static void parse_FormalPar(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Parameter();
	if (is_type(current)) {
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
	}
	else {
		parser_error(current, "Type");
	}
}

static void parse_FormalPars(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_type(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalParsList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPar });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str ==")")) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Type )");
	}
}

static void parse_FormalParsList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Parameter();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalParsList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = FormalPar });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == ")")) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", )");
	}
}

static void parse_Statement(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Statement();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IDStmt });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "if")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IfStmt });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "while")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = WhileStmt });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "foreach")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachStmt });
	} else if ((current.type == Lexer::Keyword) && (current.str == "begin")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = BlockStmt });
	}
	else {
		parser_error(current, "Identifier if while foreach begin");
	}

}

static void parse_IDStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CallStmt });
	}
	else if (((current.type == Lexer::Delimiter) && (current.str == "[")) ||
		((current.type == Lexer::Delimiter) && (current.str == ":")))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = AssignmentStmt });
	}
	else {
		parser_error(current, "( [ :");
	}
}

static void parse_AssignmentStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_AssignmentStatement();
	if (((current.type == Lexer::Delimiter) && (current.str == "[")) ||
		((current.type == Lexer::Delimiter) && (current.str == ":")))
	{
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = AssignmentExpr });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Index });

	}
	else {
		parser_error(current, "[ :");
	}
}

static void parse_AssignmentExpr(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (((current.type == Lexer::Delimiter) && (current.str == "(")) ||
		is_unaryop(current) || (current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && (current.str == "if")) ||
		is_expressionliteral(current))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehension });
	}
	else {
		parser_error(current, "if unaryOp ( Identifier expressionLiteral [");
	}
}

static void parse_CallStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_CallStatement();
	if ((current.type == Lexer::Delimiter) && (current.str == "(")) {
		keller.push_back(Symbol{ .term_str = ";", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ")", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = CallParameters });
		keller.push_back(Symbol{ .term_str = "(", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "(");
	}
}

static void parse_BlockStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_BlockStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "begin")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = BlockVarStmt });
		keller.push_back(Symbol{ .term_str = "begin", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "begin");
	}
}

static void parse_BlockVarStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "var")) {
		keller.push_back(Symbol{ .term_str = "do", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecls });
		keller.push_back(Symbol{ .term_str = "var", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && ((current.str == "begin") || (current.str == "if") || (current.str == "while") || (current.str == "foreach") || (current.str == "end"))))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "var ...");
	}

}

static void parse_IfStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_IfStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "if")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IfEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ElseStmt });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "then", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "if", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "if");
	}
}

static void parse_ElseStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "else")) {
		builder.start_ElseStatement();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "else", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endif"))) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "else end endif");
	}
}

static void parse_WhileStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_WhileStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "while")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = WhileStmtEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "do", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "while", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "while");
	}
}

static void parse_WhileStmtEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_WhileStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endwhile")) {
		keller.push_back(Symbol{ .term_str = "endwhile", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end endwhile");
	}
}

static void parse_ForeachStmt(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_ForeachStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "foreach")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachStmtEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "do", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachGeneratorList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachGenerator });
	}
	else {
		parser_error(current, "foreach");
	}
}

static void parse_ForeachGenerator(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Generator();
	if ((current.type == Lexer::Keyword) && (current.str == "foreach")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "in", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GeneratorType });
		keller.push_back(Symbol{ .term_str = "foreach", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "foreach");
	}
}

static void parse_ForeachGeneratorList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Generator();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachGeneratorList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ForeachGenerator });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "var") || (current.str =="do"))) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", ");
	}
}

static void parse_ForeachStmtEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_ForeachStatement();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endforeach")) {
		keller.push_back(Symbol{ .term_str = "endforeach", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end endforeach");
	}
}

static void parse_GeneratorType(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (is_type(current)) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Type });
	}
	else if (current.type == Lexer::Identifier) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Type Identifier");
	}
}

static void parse_Statements(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && ((current.str == "begin") || (current.str == "if") || (current.str == "while") || (current.str == "foreach"))))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statement });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "end") || (current.str == "endif") || (current.str == "endwhile") || (current.str == "else") ||
			(current.str == "endforeach") || (current.str == "endprocedure") || (current.str == "endaction") || (current.str == "endinitialize")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Identifier begin if while foreach");
	}
}

static void parse_ConstAsgn(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "=")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Keyword) &&
		((current.str == "do") || (current.str == "end") || (current.str == "begin") ||
			(current.str == "endaction") || (current.str == "endinitialize"))) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ";") || (current.str == ","))) ||
		((current.type == Lexer::Delimiter) && (current.str == ")")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "=");
	}
}

static void parse_Asgn(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter) && (current.str == ":")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = AssignmentExpr });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "=")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = AssignmentExpr });
		keller.push_back(Symbol{ .term_str = "=", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Keyword) &&
				((current.str == "do") || (current.str == "end") || (current.str == "begin") ||
				(current.str == "endaction") || (current.str == "endinitialize"))) ||
		((current.type == Lexer::Delimiter2) && ((current.str == ";") || (current.str == ","))))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ":= =");
	}
}

static void parse_Action(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_Action();
	if ((current.type == Lexer::Identifier) || ((current.type == Lexer::Keyword) && (current.str == "action"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = StatementBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionHead });
		keller.push_back(Symbol{ .term_str = "action", .type = Terminal, .term_type = Keyword });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionName });
	}
	else {
		parser_error(current, "Identifier action");
	}
}

static void parse_InitializationAction(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "initialize")) {
		builder.start_InitAction();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InitializationActionEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = StatementBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InitializerHead });
		keller.push_back(Symbol{ .term_str = "initialize", .type = Terminal, .term_type = Keyword });
	}
	else if (current.type == Lexer::Keyword &&
		((current.str == "schedule") || (current.str == "priority") || (current.str == "end") || (current.str == "endactor")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "initialize schedule priority end endactor");
	}
}

static void parse_InitializationActionEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_InitAction();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endinitialize")) {
		keller.push_back(Symbol{ .term_str = "endinitialize", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endinitialize");
	}
}

static void parse_ActionTag(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_IDList();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagIDs });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_ActionTagIDs(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ".")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTagIDs });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = ".", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Delimiter) && ((current.str == ")") || (current.str == ":"))) ||
		((current.type == Lexer::Operator) && (current.str == ">")) || ((current.type == Lexer::Delimiter2) && (current.str ==";")))
	{
		builder.end_IDList();
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ". ) : > ;");
	}
}

static void parse_ActionEnd(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_Action();
	if ((current.type == Lexer::Keyword) && (current.str == "end")) {
		keller.push_back(Symbol{ .term_str = "end", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && (current.str == "endaction")) {
		keller.push_back(Symbol{ .term_str = "endaction", .type = Terminal, .term_type = Keyword });
	}
	else {
		parser_error(current, "end, endaction");
	}
}

static void parse_ActionHead(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Identifier) || ((current.type == Lexer::Operator) && (current.str == "==>"))) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GuardBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressions });
		keller.push_back(Symbol{ .term_str = "==>", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InputPatterns });
	}
	else {
		parser_error(current, "Identifier ==>");
	}
}

static void parse_IDs(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IDList });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_IDList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IDList });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if (((current.type == Lexer::Keyword) && (current.str =="in")) || ((current.type == Lexer::Delimiter) && (current.str == "]"))) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "");
	}
}

static void parse_GuardBlock(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "guard")) {
		builder.start_Guard();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GuardEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expressions });
		keller.push_back(Symbol{ .term_str = "guard", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "var") || (current.str == "do") || (current.str == "end") ||
			(current.str == "endaction") || (current.str == "endinitialize")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "guard var do end endaction endinitialize");
	}
}

static void parse_VarBlock(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "var")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarDecls });
		keller.push_back(Symbol{ .term_str = "var", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "do") || (current.str == "end") || (current.str == "endaction") || (current.str == "endinitialize") ||
			(current.str == "begin")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "guard var do end endaction endinitialize begin");
	}
}

static void parse_ActionName(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ActionTag });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "action") || (current.str == "initialize"))) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Identifier action initialize");
	}
}

static void parse_StatementBlock(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "do")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Statements });
		keller.push_back(Symbol{ .term_str = "do", .type = Terminal, .term_type = Keyword });
	}
	else if ((current.type == Lexer::Keyword) && ((current.str == "end") || (current.str == "endaction") || (current.str == "endinitialize"))) {
		/* nothing to do, follow set, empty action */
	}
	else {
		parser_error(current, "do end endaction endinitialize");
	}
}

static void parse_InputPattern(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_InputPattern();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = RepeatClause });
		keller.push_back(Symbol{ .term_str = "]", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = IDs });
		keller.push_back(Symbol{ .term_str = "[", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_InputPatterns(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InputPatternList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InputPattern });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "==>")) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Identifier ==>");
	}
}

static void parse_InputPatternList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_InputPattern();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InputPatternList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = InputPattern });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Operator) && (current.str == "==>")) {
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", ==>");
	}
}

static void parse_OutputExpression(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.start_OutputExpression();
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = RepeatClause });
		keller.push_back(Symbol{ .term_str = "]", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionContent });
		keller.push_back(Symbol{ .term_str = "[", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = ":", .type = Terminal, .term_type = Special });
		keller.push_back(Symbol{ .term_str = "ID", .type = Terminal, .term_type = ID });
	}
	else {
		parser_error(current, "Identifier");
	}
}

static void parse_OutputExpressions(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (current.type == Lexer::Identifier) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpression });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "var") || (current.str == "guard") || (current.str == "do") || (current.str == "end") || (current.str == "endaction") ||
			(current.str == "endinitialize")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "Identifier var guard do end endaction endinitialize");
	}
}

static void parse_OutputExpressionList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	builder.end_OutputExpression();
	if ((current.type == Lexer::Delimiter2) && (current.str == ",")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpression });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Keyword) &&
		((current.str == "var") || (current.str == "guard") || (current.str == "do") || (current.str == "end") || (current.str == "endaction") ||
			(current.str == "endinitialize")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, ", var guard do end endaction endinitialize");
	}
}

static void parse_OutputExpressionContent(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if (((current.type == Lexer::Delimiter) && (current.str == "(")) ||
		is_unaryop(current) || (current.type == Lexer::Identifier) ||
		((current.type == Lexer::Keyword) && (current.str == "if")) ||
		is_expressionliteral(current))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionContentList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Expression });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "[")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionContentList });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ListComprehension });
	}
	else {
		parser_error(current, "if unaryOp ( Identifier expressionLiteral [");
	}
}

static void parse_OutputExpressionContentList(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Delimiter2) && (current.str == ","))
	{
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressionContent });
		keller.push_back(Symbol{ .term_str = ",", .type = Terminal, .term_type = Special });
	}
	else if ((current.type == Lexer::Delimiter) && (current.str == "]")) {
		/* follow set, nothing to do */
	}
	else {
		parser_error(current, ", ]");
	}
}

static void parse_RepeatClause(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Keyword) && (current.str == "repeat")) {
		builder.start_Repeat();
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = RepeatEnd });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = ConstCalc });
		keller.push_back(Symbol{ .term_str = "repeat", .type = Terminal, .term_type = Keyword });
	}
	else if (((current.type == Lexer::Keyword) &&
		((current.str == "var") || (current.str == "guard") || (current.str == "do") || (current.str == "end") || (current.str == "endaction") ||
			(current.str == "endinitialize"))) ||
		((current.type == Lexer::Delimiter2) && (current.str == ",")) ||
		((current.type == Lexer::Operator) && (current.str =="==>")))
	{
		/* nothing to do, follow set, empty */
	}
	else {
		parser_error(current, "repeat , ==> var guard do end endaction endinitialize");
	}
}

static void parse_InitializerHead(
	std::vector<Parser::Symbol>& keller,
	Lexer::Token current,
	AST::AST_Builder& builder)
{
	if ((current.type == Lexer::Operator) && (current.str == "==>")) {
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = VarBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = GuardBlock });
		keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = OutputExpressions });
		keller.push_back(Symbol{ .term_str = "==>", .type = Terminal, .term_type = Special });
	}
	else {
		parser_error(current, "==>");
	}
}

/* Interface */
AST::AST_Root* Parser_Class::parse(void)
{
	current = lexer->next();
	keller.push_back(Symbol{ .type = Nonterminal, .nonterm_type = Start });

	while (current.str != Lexer::END && !keller.empty()) {

		//std::cout << "Current token: " << current.str << std::endl;

		if (keller.back().type == Terminal) {
			if ((current.str == keller.back().term_str) ||
				((keller.back().term_type == ID) && (current.type == Lexer::Identifier) ||
				((keller.back().term_type == Number) && (current.type == Lexer::NumericLiteral)) ||
				((keller.back().term_type == String) && (current.type == Lexer::String))))
			{
				builder.add_token(current);
				current = lexer->next();
				keller.pop_back();
				continue;
			}
			else {
				parser_error(current, keller.back().term_str);
			}
		}
		else {
			//std::cout << "Nonterm: ";
			//print_nonterm_type(keller.back().nonterm_type);
			assert(keller.back().type == Nonterminal);
			auto keller_top = keller.back();
			keller.pop_back();
			switch (keller_top.nonterm_type) {
			case Start: parse_Start(keller, current, builder);  break;
			case CALFile: parse_CALFile(keller, current, builder); break;
			case Package: parse_Package(keller, current, builder); break;
			case CALFileContent: parse_CALFileContent(keller, current, builder); break;
			case Imports: parse_Imports(keller, current, builder); break;
			case Import: parse_Import(keller, current, builder); break;
			case Unit: parse_Unit(keller, current, builder); break;
			case UnitBody: parse_UnitBody(keller, current, builder); break;
			case UnitEnd: parse_UnitEnd(keller, current, builder); break;
			case Actor: parse_Actor(keller, current, builder); break;
			case ActorEnd: parse_ActorEnd(keller, current, builder); break;
			case ActorBody: parse_ActorBody(keller, current, builder); break;
			case IOSig: parse_IOSig(keller, current, builder); break;
			case QualID: parse_QualID(keller, current, builder); break;
			case QualIDList: parse_QualIDList(keller, current, builder); break;
			case QualIDListElement: parse_QualIDListElement(keller, current, builder); break;
			case ActorPars: parse_ActorPars(keller, current, builder); break;
			case ActorPar: parse_ActorPar(keller, current, builder); break;
			case ActorParsList: parse_ActorParsList(keller, current, builder); break;
			case PortDecl: parse_PortDecl(keller, current, builder); break;
			case PortDecls: parse_PortDecls(keller, current, builder); break;
			case PortDeclList: parse_PortDeclList(keller, current, builder); break;
			case ActionSchedule: parse_ActionSchedule(keller, current, builder); break;
			case ScheduleFSM: parse_ScheduleFSM(keller, current, builder); break;
			case ScheduleFSMEnd: parse_ScheduleFSMEnd(keller, current, builder); break;
			case StateTransitionList: parse_StateTransitionList(keller, current, builder); break;
			case StateTransition: parse_StateTransition(keller, current, builder); break;
			case ActionTags: parse_ActionTags(keller, current, builder); break;
			case ActionTagList: parse_ActionTagList(keller, current, builder); break;
			case PriorityOrder: parse_PriorityOrder(keller, current, builder); break;
			case PriorityInequality: parse_PriorityInequality(keller, current, builder); break;
			case PriorityInequalityList: parse_PriorityInequalityList(keller, current, builder); break;
			case ActionTagInequalityList: parse_ActionTagInequalityList(keller, current, builder); break;
			case Type: parse_Type(keller, current, builder); break;
			case IntegerType: parse_IntegerType(keller, current, builder); break;
			case IntegerTypes: parse_IntegerTypes(keller, current, builder); break;
			case Size: parse_Size(keller, current, builder); break;
			case ListType: parse_ListType(keller, current, builder); break;
			case FloatType: parse_FloatType(keller, current, builder); break;
			case FloatTypes: parse_FloatTypes(keller, current, builder); break;
			case BooleanOperator: parse_BooleanOperator(keller, current, builder); break;
			case ArithOperator: parse_ArithOperator(keller, current, builder); break;
			case BitOperator: parse_BitOperator(keller, current, builder); break;
			case relOperator: parse_relOperator(keller, current, builder); break;
			case unaryBooleanOperator: parse_unaryBooleanOperator(keller, current, builder); break;
			case unaryBitOperator: parse_unaryBitOperator(keller, current, builder); break;
			case unaryArithOperator: parse_unaryArithOperator(keller, current, builder); break;
			case Op: parse_Op(keller, current, builder); break;
			case unaryOp: parse_unaryOp(keller, current, builder); break;
			case Expression: parse_Expression(keller, current, builder); break;
			case Expressions: parse_Expressions(keller, current, builder); break;
			case ExpressionList: parse_ExpressionList(keller, current, builder); break;
			case ConstCalc: parse_ConstCalc(keller, current, builder); break;
			case SingleConstCalc: parse_SingleConstCalc(keller, current, builder); break;
			case SingleConstCalcList: parse_SingleConstCalcList(keller, current, builder); break;
			case SingleExpression: parse_SingleExpression(keller, current, builder); break;
			case SingleExpressionAdd: parse_SingleExpressionAdd(keller, current, builder); break;
			case SingleExpressionList: parse_SingleExpressionList(keller, current, builder); break;
			case Negation: parse_Negation(keller, current, builder); break;
			case ExpressionLiteral: parse_ExpressionLiteral(keller, current, builder); break;
			case IfExpression: parse_IfExpression(keller, current, builder); break;
			case IfEnd: parse_IfEnd(keller, current, builder); break;
			case ListComprehension: parse_ListComprehension(keller, current, builder); break;
			case ListComprehensionContent: parse_ListComprehensionContent(keller, current, builder); break;
			case ListComprehensionGenerator: parse_ListComprehensionGenerator(keller, current, builder); break;
			case ListComprehensionExprs: parse_ListComprehensionExprs(keller, current, builder); break;
			case ListComprehensionExprsList: parse_ListComprehensionExprsList(keller, current, builder); break;
			case Generator: parse_Generator(keller, current, builder); break;
			case Generators: parse_Generators(keller, current, builder); break;
			case GeneratorList: parse_GeneratorList(keller, current, builder); break;
			case CallExpr: parse_CallExpr(keller, current, builder); break;
			case CallParameters: parse_CallParameters(keller, current, builder); break;
			case Index: parse_Index(keller, current, builder); break;
			case FuncDecl: parse_FuncDecl(keller, current, builder); break;
			case FuncDeclEnd: parse_FuncDeclEnd(keller, current, builder); break;
			case ProcDecl: parse_ProcDecl(keller, current, builder); break;
			case ProcDeclEnd: parse_ProcDeclEnd(keller, current, builder); break;
			case NativeDecl: parse_NativeDecl(keller, current, builder); break;
			case NativeDeclDes: parse_NativeDeclDes(keller, current, builder); break;
			case NativeFuncDecl: parse_NativeFuncDecl(keller, current, builder); break;
			case NativeProcedureDecl: parse_NativeProcedureDecl(keller, current, builder); break;
			case VarDecl: parse_VarDecl(keller, current, builder); break;
			case VarDecls: parse_VarDecls(keller, current, builder); break;
			case VarDeclList: parse_VarDeclList(keller, current, builder); break;
			case ArrayDef: parse_ArrayDef(keller, current, builder); break;
			case FormalPar: parse_FormalPar(keller, current, builder); break;
			case FormalPars: parse_FormalPars(keller, current, builder); break;
			case FormalParsList: parse_FormalParsList(keller, current, builder); break;
			case Statement: parse_Statement(keller, current, builder); break;
			case IDStmt: parse_IDStmt(keller, current, builder); break;
			case AssignmentStmt: parse_AssignmentStmt(keller, current, builder); break;
			case AssignmentExpr: parse_AssignmentExpr(keller, current, builder); break;
			case CallStmt: parse_CallStmt(keller, current, builder); break;
			case BlockStmt: parse_BlockStmt(keller, current, builder); break;
			case BlockVarStmt: parse_BlockVarStmt(keller, current, builder); break;
			case IfStmt: parse_IfStmt(keller, current, builder); break;
			case ElseStmt: parse_ElseStmt(keller, current, builder); break;
			case WhileStmt: parse_WhileStmt(keller, current, builder); break;
			case WhileStmtEnd: parse_WhileStmtEnd(keller, current, builder); break;
			case ForeachStmt: parse_ForeachStmt(keller, current, builder); break;
			case ForeachGenerator: parse_ForeachGenerator(keller, current, builder); break;
			case ForeachGeneratorList: parse_ForeachGeneratorList(keller, current, builder); break;
			case ForeachStmtEnd: parse_ForeachStmtEnd(keller, current, builder); break;
			case GeneratorType: parse_GeneratorType(keller, current, builder); break;
			case Statements: parse_Statements(keller, current, builder); break;
			case ConstAsgn: parse_ConstAsgn(keller, current, builder); break;
			case Asgn: parse_Asgn(keller, current, builder); break;
			case Action: parse_Action(keller, current, builder); break;
			case InitializationAction: parse_InitializationAction(keller, current, builder); break;
			case InitializationActionEnd: parse_InitializationActionEnd(keller, current, builder); break;
			case ActionTag: parse_ActionTag(keller, current, builder); break;
			case ActionTagIDs: parse_ActionTagIDs(keller, current, builder); break;
			case ActionEnd: parse_ActionEnd(keller, current, builder); break;
			case ActionHead: parse_ActionHead(keller, current, builder); break;
			case IDs: parse_IDs(keller, current, builder); break;
			case IDList: parse_IDList(keller, current, builder); break;
			case GuardBlock: parse_GuardBlock(keller, current, builder); break;
			case VarBlock: parse_VarBlock(keller, current, builder); break;
			case ActionName: parse_ActionName(keller, current, builder); break;
			case StatementBlock: parse_StatementBlock(keller, current, builder); break;
			case InputPattern: parse_InputPattern(keller, current, builder); break;
			case InputPatterns: parse_InputPatterns(keller, current, builder); break;
			case InputPatternList: parse_InputPatternList(keller, current, builder); break;
			case OutputExpression: parse_OutputExpression(keller, current, builder); break;
			case OutputExpressions: parse_OutputExpressions(keller, current, builder); break;
			case OutputExpressionList: parse_OutputExpressionList(keller, current, builder); break;
			case OutputExpressionContent: parse_OutputExpressionContent(keller, current, builder); break;
			case OutputExpressionContentList: parse_OutputExpressionContentList(keller, current, builder); break;
			case RepeatClause: parse_RepeatClause(keller, current, builder); break;
			case InitializerHead: parse_InitializerHead(keller, current, builder); break;
			case IndexEnd: builder.end_Index(); break;
			case TypeEnd: builder.end_Type(); break;
			case GuardEnd: builder.end_Guard(); break;
			case RepeatEnd: builder.end_Repeat(); break;
			case CallExpressionEnd: builder.end_CallExpression(); break;
			case LiteralEnd: builder.end_Literal(); break;
			case UnaryOpEnd: builder.end_UnaryOp(); break;
			case OpEnd: builder.end_Operator(); break;
			default:
				std::cout << "Unknown nonterminal." << std::endl;
				exit(2);
				break;
			}
		}
	}

	if (!keller.empty()) {
		std::cout << "Parser in state: ";
		print_nonterm_type(keller.back().nonterm_type);
		std::cout << " but no further tokens can be provided by the lexer." << std::endl;
		exit(4);
	}
	else if (current.str != Lexer::END) {
		std::cout << "Parsing completed but the input still contains unexpected trailing tokens.\n";
		std::cout << "Current token: " << current.str << std::endl;
		exit(4);
	}
	else {
		return builder.get_ast();
	}
}