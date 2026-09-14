#pragma once

#include <string>
#include <vector>

namespace AST {

	class AST_Element {
	public:
		unsigned line = 0;
		unsigned character = 0;
		
		unsigned check_val = 0;
	};

	class ID : public AST_Element {
	public:
		std::string name;
	};

	class BaseExpression : public AST_Element {
	public:
		virtual ~BaseExpression() {};

		virtual BaseExpression* clone() const = 0;
	};

	class Expression : public BaseExpression {
	public:
		BaseExpression* child;

		bool brakets;

		Expression* clone() const {
			return new Expression(*this);
		}

		Expression() { brakets = false; child = nullptr; };

		Expression(const Expression& c) {
			brakets = c.brakets;
			child = (c.child != nullptr) ? c.child->clone() : nullptr;
		}
	};

	class Type : public AST_Element {
	public:
		ID type;
		Expression* size;
		Type *listtype;
		bool non_standard_type = false;

		Type() { size = nullptr; listtype = nullptr; };

		Type(Type& c) {
			type = c.type;
			size = (c.size != nullptr) ? new Expression{ *c.size } : nullptr;
			listtype = (c.listtype != nullptr) ? new Type{ *c.listtype } : nullptr;
			non_standard_type = c.non_standard_type;
		}
	};

	class VarDefinition : public AST_Element {
	public:
		Type type;
		ID name;
		std::vector<Expression*> arrays;
		bool constassign;
		Expression* assign;

		VarDefinition() { assign = nullptr; constassign = true; };

		VarDefinition(const VarDefinition& c) {
			type = c.type;
			name = c.name;
			for (auto a : c.arrays) {
				arrays.push_back(new Expression{ *a });
			}
			constassign = c.constassign;
			assign = (c.assign != nullptr) ? new Expression{ *c.assign } : nullptr;
		}
	};

	class Index : public AST_Element {
	public:
		Expression* index;

		Index() { index = nullptr; }

		Index(const Index& c) {
			index = (c.index != nullptr) ? new Expression{ *c.index } : nullptr;
		}
	};

	class UnaryOperator : public AST_Element {
	public:
		std::string ops;
	};

	class Call : public AST_Element {
	public:
		std::vector<Expression*> parameters;

		Call() {}

		Call(const Call& c) {
			for (auto x : c.parameters) {
				parameters.push_back(new Expression{ *x });
			}
		}
	};

	class Operand : public BaseExpression {
	public:
		virtual ~Operand() {};
	};

	class Literal : public Operand {
	public:
		bool negation;
		std::string literal;

		Literal* clone() const {
			return new Literal(*this);
		}

		Literal() { negation = false; };

		Literal(const Literal& c) {
			negation = c.negation;
			literal = c.literal;
		}
	};

	class Identifier : public Operand {
	public:
		std::string identifier;
		UnaryOperator* unary_left;
		UnaryOperator* unary_right;
		std::vector<Index*> indices;
		Call* call;

		Identifier* clone() const {
			return new Identifier(*this);
		}

		Identifier() { unary_left = nullptr; unary_right = nullptr; call = nullptr; };

		Identifier(const Identifier& c) {
			identifier = c.identifier;
			unary_left = (c.unary_left != nullptr) ? new UnaryOperator{ *c.unary_left } : nullptr;
			unary_right = (c.unary_right != nullptr) ? new UnaryOperator{ *c.unary_right } : nullptr;
			for (auto i : c.indices) {
				indices.push_back(new Index{*i});
			}
			call = (c.call != nullptr) ? new Call{ *c.call } : nullptr;
		}
	};

	class FSM_Enumeration_Element : public Operand {
	public:
		std::string enum_name;
		std::string enum_element;

		FSM_Enumeration_Element* clone() const {
			return new FSM_Enumeration_Element(*this);
		}

		FSM_Enumeration_Element() {};

		FSM_Enumeration_Element(const FSM_Enumeration_Element& c) {
			enum_name = c.enum_name;
			enum_element = c.enum_element;
		}
	};

	class PortPreview : public Operand {
	public:
		std::string port;
		std::string type;
		std::string prev_identifier;
		Index* index;


		PortPreview* clone() const {
			return new PortPreview(*this);
		}

		PortPreview() { index = nullptr; }

		PortPreview(const PortPreview& c) {
			port = c.port;
			type = c.type;
			prev_identifier = c.prev_identifier;
			index = (c.index != nullptr) ? new Index{ *c.index } : nullptr;
		}
	};

	class PortSize : public Operand {
	public:
		std::string port;
		std::string type;

		PortSize* clone() const {
			return new PortSize(*this);
		}
	};

	class PortFree : public Operand {
	public:
		std::string port;
		std::string type;

		PortFree* clone() const {
			return new PortFree(*this);
		}
	};

	/* Yes, it is basically an operator, but also comprising all the childs and
	 * therefore it is an operand again.
	 */
	class TernaryOperator : public Operand {
	public:
		Expression* cond;
		Expression* ifblock;
		Expression* elseblock;

		TernaryOperator* clone() const {
			return new TernaryOperator(*this);
		}

		TernaryOperator() { cond = nullptr; ifblock = nullptr; elseblock = nullptr; };

		TernaryOperator(const TernaryOperator& c) {
			cond = (c.cond != nullptr) ? new Expression{ *c.cond } : nullptr;
			ifblock = (c.ifblock != nullptr) ? new Expression{ *c.ifblock } : nullptr;
			elseblock = (c.elseblock != nullptr) ? new Expression{ *c.elseblock } : nullptr;
		}
	};

	class Operator : public BaseExpression {
	public:
		std::string ops;

		BaseExpression* left;
		BaseExpression* right;

		Operator* clone() const {
			return new Operator(*this);
		}

		Operator() { left = nullptr; right = nullptr; };

		Operator(const Operator& c) {
			ops = c.ops;
			left = (c.left != nullptr) ? c.left->clone() : nullptr;
			right = (c.right != nullptr) ? c.right->clone() : nullptr;
		}
	};

	class Generator : public AST_Element {
	public:
		Type* type;
		ID identifier;
		Expression* start;
		Expression* end;

		Generator() { type = nullptr; start = nullptr; end = nullptr; };

		Generator(Generator& c) {
			type = (c.type != nullptr) ? new Type{ *c.type } : nullptr;
			identifier = c.identifier;
			start = (c.start != nullptr) ? new Expression{ *c.start } : nullptr;
			end = (c.end != nullptr) ? new Expression{ *c.end } : nullptr;
		}
	};

	class ListComprehension : public BaseExpression {
	public:
		std::vector<Expression*> expressions;
		std::vector<Generator*> generators;

		ListComprehension* clone() const {
			return new ListComprehension(*this);
		}

		ListComprehension() {}

		ListComprehension(const ListComprehension& c) {
			for (auto e : c.expressions) {
				expressions.push_back(new Expression{ *e });
			}
			for (auto g : c.generators) {
				generators.push_back(new Generator{ *g });
			}
		}
	};

	class Statement : public AST_Element {
	public:
		virtual ~Statement() {}

		virtual Statement* clone() const = 0;
	};

	class ElseStatement : public Statement {
	public:
		std::vector<Statement*> statements;

		ElseStatement* clone() const {
			return new ElseStatement(*this);
		}

		ElseStatement() {}

		ElseStatement(const ElseStatement& c) {
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class IfStatement : public Statement {
	public:
		Expression* condition;
		std::vector<Statement*> ifblock;
		ElseStatement *elseblock;

		IfStatement* clone() const {
			return new IfStatement(*this);
		}

		IfStatement() { condition = nullptr; elseblock = nullptr; };

		IfStatement(const IfStatement& c) : Statement(c) {
			condition = (c.condition != nullptr) ? new Expression{ *c.condition } : nullptr;
			for (auto x : c.ifblock) {
				ifblock.push_back(x->clone());
			}
			elseblock = (c.elseblock != nullptr) ? new ElseStatement{ *c.elseblock } : nullptr;
		}
	};

	class CallStatement : public Statement {
	public:
		ID name;
		std::vector<Expression*> parameters;

		CallStatement* clone() const {
			return new CallStatement(*this);
		}

		CallStatement() {}

		CallStatement(const CallStatement& c) {
			name = c.name;
			for (auto p : c.parameters) {
				parameters.push_back(new Expression{ *p });
			}
		}
	};

	class BlockStatement : public Statement {
	public:
		std::vector<VarDefinition*> vars;
		std::vector<Statement*> statements;

		BlockStatement* clone() const {
			return new BlockStatement(*this);
		}

		BlockStatement() {}

		BlockStatement(const BlockStatement& c) {
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class WhileStatement : public Statement {
	public:
		Expression* condition;
		std::vector<VarDefinition*> vars;
		std::vector<Statement*> statements;

		WhileStatement* clone() const {
			return new WhileStatement(*this);
		}

		WhileStatement() { condition = nullptr; }

		WhileStatement(const WhileStatement& c) {
			condition = (c.condition != nullptr) ? new Expression{ *c.condition } : nullptr;
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class ForeachStatement : public Statement {
	public:
		std::vector<Generator*> generators;
		std::vector<VarDefinition*> vars;
		std::vector<Statement*> statements;

		ForeachStatement* clone() const {
			return new ForeachStatement(*this);
		}

		ForeachStatement() {}

		ForeachStatement(const ForeachStatement& c) {
			for (auto g : c.generators) {
				generators.push_back(new Generator{ *g });
			}
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class OutputChannelWriteStatement : public Statement {
	public:
		ID port;
		Expression* expr;

		OutputChannelWriteStatement* clone() const {
			return new OutputChannelWriteStatement(*this);
		}

		OutputChannelWriteStatement() { expr = nullptr; };

		OutputChannelWriteStatement(const OutputChannelWriteStatement& c) {
			port = c.port;
			expr = (c.expr != nullptr) ? new Expression{ *c.expr } : nullptr;
		}
	};

	class InputChannelReadStatement : public Statement {
	public:
		ID port;
		ID identifier;
		Index* index;

		/* Optimizer information */
		unsigned offset;
		unsigned size;

		InputChannelReadStatement* clone() const {
			return new InputChannelReadStatement(*this);
		}

		InputChannelReadStatement() { index = nullptr; offset = 0; size = 0; };

		InputChannelReadStatement(const InputChannelReadStatement& c) {
			port = c.port;
			identifier = c.identifier;
			index = (c.index != nullptr) ? new Index{ *c.index } : nullptr;
			offset = c.offset;
			size = c.size;
		}
	};

	class AssignmentStatement : public Statement {
	public:
		bool constasgn;
		ID identifier;
		std::vector<Index*> indices;
		Expression* asgnvalue;

		AssignmentStatement* clone() const {
			return new AssignmentStatement(*this);
		}

		AssignmentStatement() { constasgn = true; asgnvalue = nullptr; };

		AssignmentStatement(const AssignmentStatement& c) {
			constasgn = c.constasgn;
			identifier = c.identifier;
			for (auto i : c.indices) {
				auto q = new Index{ *i };
				indices.push_back(q);
			}
			asgnvalue = (c.asgnvalue != nullptr) ? new Expression{ *c.asgnvalue } : nullptr;
		}
	};

	class ReturnStatement : public Statement {
	public:
		ReturnStatement* clone() const {
			return new ReturnStatement(*this);
		}

		ReturnStatement() { };

		ReturnStatement(const ReturnStatement& c) {};
	};

	class TerminateLoopStatement : public Statement {
	public:
		TerminateLoopStatement* clone() const {
			return new TerminateLoopStatement(*this);
		}

		TerminateLoopStatement() {};

		TerminateLoopStatement(const TerminateLoopStatement& c) {};
	};

	class Schedule_FSM_State : public AST_Element {
	public:
		ID state;
		std::vector<ID> actions;
		ID next;

		/* Information for further processing */
		std::vector< std::pair<std::string, std::string> > actual_actions;

		Schedule_FSM_State() {};

		Schedule_FSM_State(const Schedule_FSM_State& c) {
			state = c.state;
			next = c.next;
			for (auto a : c.actions) {
				actions.push_back(a);
			}
			for (auto a : c.actual_actions) {
				actual_actions.push_back(a);
			}
		}
	};

	class Schedule_FSM : public AST_Element {
	public:
		ID inital_state;
		std::vector<Schedule_FSM_State*> transitions;

		Schedule_FSM() {};

		Schedule_FSM(const Schedule_FSM& c) {
			inital_state = c.inital_state;
			for (auto t : c.transitions) {
				transitions.push_back(new Schedule_FSM_State{ *t });
			}
		}
	};

	class FSM_Enumeration : public AST_Element {
	public:
		std::string name;
		std::string inital_state;
		std::vector<std::string> states;

		FSM_Enumeration() {};

		FSM_Enumeration(const FSM_Enumeration& c) {
			name = c.name;
			inital_state = c.inital_state;
			for (auto s : c.states) {
				states.push_back(s);
			}
		}
	};

	class Action_Priority : public AST_Element {
	public:
		std::vector<ID> prio_rel;

		Action_Priority() {};

		Action_Priority(const Action_Priority& c) {
			for (auto p : c.prio_rel) {
				prio_rel.push_back(p);
			}
		}
	};

	class Input_Pattern : public AST_Element {
	public:
		ID port;
		std::vector<ID> IDs;
		Expression* repeat;

		Input_Pattern() { repeat = nullptr; };

		Input_Pattern(const Input_Pattern& c) {
			port = c.port;
			if (c.repeat != nullptr) {
				repeat = new Expression{ *c.repeat };
			}
			else {
				repeat = nullptr;
			}
			for (auto i : c.IDs) {
				IDs.push_back(i);
			}
		}
	};

	class Output_Expression : public AST_Element {
	public:
		ID port;
		std::vector<Expression*> expressions;
		Expression* repeat;

		Output_Expression() { repeat = nullptr; };

		Output_Expression(const Output_Expression& c) {
			port = c.port;
			for (auto e : c.expressions) {
				expressions.push_back(new Expression{ *e });
			}
			if (c.repeat != nullptr) {
				repeat = new Expression{ *c.repeat };
			}
			else {
				repeat = nullptr;
			}
		}
	};

	class Port : public AST_Element {
	public:
		Type type;
		ID name;

		Port() {};

		Port(const Port& c) {
			type = c.type;
			name = c.name;
		}
	};

	class Parameter : public AST_Element {
	public:
		Type type;
		ID name;

		Parameter() {};

		Parameter(const Parameter& c) {
			type = c.type;
			name = c.name;
		}
	};

	class Action : public AST_Element {
	public:
		ID name;

		std::vector<Expression*> guards;
		std::vector<Expression*> output_guards; /* Free checks, only used for optimization! */

		std::vector<Input_Pattern*> input_patterns;
		std::vector<Output_Expression*> output_expressions;

		std::vector<VarDefinition*> vars;
		std::vector<Statement*> statements;

		Action() {};

		Action(const Action& c) {
			name = c.name;
			for (auto g : c.guards) {
				guards.push_back(new Expression{ *g });
			}
			for (auto o : c.output_guards) {
				output_guards.push_back(new Expression{ *o });
			}
			for (auto i : c.input_patterns) {
				input_patterns.push_back(new Input_Pattern{ *i });
			}
			for (auto o : c.output_expressions) {
				output_expressions.push_back(new Output_Expression{ *o });
			}
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class Function : public AST_Element {
	public:
		ID name;
		std::vector<Parameter*> parameters;
		Type ret_type;
		Expression* expression;

		Function() { expression = nullptr; };

		Function(const Function& c) {
			name = c.name;
			ret_type = c.ret_type;
			for (auto p : c.parameters) {
				parameters.push_back(new Parameter{ *p });
			}
			expression = (c.expression != nullptr) ? new Expression{ *c.expression } : nullptr;
		}
	};

	class NativeFunction : public AST_Element {
	public:
		ID name;
		std::vector<Parameter*> parameters;
		Type ret_type;

		NativeFunction() {};

		NativeFunction(const NativeFunction& c) {
			name = c.name;
			ret_type = c.ret_type;
			for (auto p : c.parameters) {
				parameters.push_back(new Parameter{ *p });
			}
		}
	};

	class NativeProcedure : public AST_Element {
	public:
		ID name;
		std::vector<Parameter*> parameters;

		NativeProcedure() {};

		NativeProcedure(const NativeProcedure& c) {
			name = c.name;
			for (auto p : c.parameters) {
				parameters.push_back(new Parameter{ *p });
			}
		}
	};

	class Procedure : public AST_Element {
	public:
		ID name;
		std::vector<Parameter*> parameters;
		std::vector<VarDefinition*> vars;
		std::vector<Statement*> statements;

		Procedure() {};

		Procedure(const Procedure& c) {
			name = c.name;
			for (auto p : c.parameters) {
				parameters.push_back(new Parameter{ *p });
			}
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto s : c.statements) {
				statements.push_back(s->clone());
			}
		}
	};

	class Import : public AST_Element {
	public:
		ID path;
		ID symbol;

		Import() {};

		Import(const Import& c) {
			path = c.path;
			symbol = c.symbol;
		}
	};

	class ActorParameter : public AST_Element {
	public:
		Type type;
		ID name;
		Expression* asign;

		ActorParameter() { asign = nullptr; };

		ActorParameter(const ActorParameter& c) {
			type = c.type;
			name = c.name;
			if (c.asign != nullptr) {
				asign = new Expression{ *c.asign };
			}
			else {
				asign = nullptr;
			}
		}
	};

	class Actor : public AST_Element {
	public:
		ID name;
		std::vector<Action*> actions;
		Schedule_FSM *fsm;
		std::vector<Action_Priority*> prios;
		Action* init;
		std::vector<FSM_Enumeration*> fsm_enums;

		std::vector<NativeFunction*> nativefunctions;
		std::vector<NativeProcedure*> nativeprocedures;
		std::vector<Function*> functions;
		std::vector<Procedure*> procedures;
		std::vector<VarDefinition*> vars;

		std::vector<ActorParameter*> parameters;

		std::vector<Port*> inports;
		std::vector<Port*> outports;

		Actor() { fsm = nullptr; init = nullptr; };

		Actor(const Actor& c) {
			name = c.name;
			if (c.fsm != nullptr) {
				fsm = new Schedule_FSM{ *c.fsm };
			}
			else {
				fsm = nullptr;
			}
			for (auto p : c.prios) {
				prios.push_back(new Action_Priority{ *p });
			}
			if (c.init != nullptr) {
				init = new Action{ *c.init };
			}
			else {
				init = nullptr;
			}
			for (auto f : c.fsm_enums) {
				fsm_enums.push_back(new FSM_Enumeration{ *f });
			}
			for (auto n : c.nativefunctions) {
				nativefunctions.push_back(new NativeFunction{ *n });
			}
			for (auto n : c.nativeprocedures) {
				nativeprocedures.push_back(new NativeProcedure{ *n });
			}
			for (auto f : c.functions) {
				functions.push_back(new Function{ *f });
			}
			for (auto p : c.procedures) {
				procedures.push_back(new Procedure{ *p });
			}
			for (auto p : c.parameters) {
				parameters.push_back(new ActorParameter{ *p });
			}
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto p : c.inports) {
				inports.push_back(new Port{ *p });
			}
			for (auto p : c.outports) {
				outports.push_back(new Port{ *p });
			}
			for (auto a : c.actions) {
				actions.push_back(new Action{ *a });
			}
		}
	};

	class Unit : public AST_Element {
	public:
		ID name;
		std::vector<Function*> functions;
		std::vector<Procedure*> procedures;
		std::vector<VarDefinition*> vars;
		std::vector<NativeFunction*> nativefunctions;
		std::vector<NativeProcedure*> nativeprocedures;

		Unit() {};

		Unit(const Unit& c) {
			name = c.name;
			for (auto f : c.functions) {
				functions.push_back(new Function{ *f });
			}
			for (auto p : c.procedures) {
				procedures.push_back(new Procedure{ *p });
			}
			for (auto v : c.vars) {
				vars.push_back(new VarDefinition{ *v });
			}
			for (auto n : c.nativefunctions) {
				nativefunctions.push_back(new NativeFunction{ *n });
			}
			for (auto n : c.nativeprocedures) {
				nativeprocedures.push_back(new NativeProcedure{ *n });
			}
		}
	};

	class AST_Root {
	public:
		std::vector<Import*> imports;
		Actor* actor;
		Unit *unit;

		AST_Root() { actor = nullptr; unit = nullptr; };

		AST_Root(const AST_Root& c) {
			for (auto i : c.imports) {
				imports.push_back(new Import{ *i });
			}
			if (c.actor != nullptr) {
				actor = new Actor{ *c.actor };
			}
			else {
				actor = nullptr;
			}
			if (c.unit != nullptr) {
				unit = new Unit{ *c.unit };
			}
			else {
				unit = nullptr;
			}
		}
	};
}