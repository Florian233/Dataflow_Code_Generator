#include "AST_Printer.hpp"

#include <iostream>
#include <string>
#include <assert.h>

static void print_expression(AST::Expression* e);
static void decode_baseexpression(AST::BaseExpression* b);
static void print_type(AST::Type t);
static void print_vardef(AST::VarDefinition* vardef, std::string prefix);

static const unsigned AST_PRINT_MAX_DEPTH = 3333;
static unsigned ast_print_depth = 0;

static void print_identifier(AST::Identifier* i)
{
	if (i->unary_left != nullptr) {
		std::cout << i->unary_left->ops;
	}
	std::cout << i->identifier;
	if (i->identifier.empty()) std::cout << "EMPTY";
	if (i->unary_right != nullptr) {
		std::cout << i->unary_right->ops;
	}
	for(auto x : i->indices) {
		std::cout << "[";
		print_expression(x->index);
		std::cout << "]";
	}
	if (i->call != nullptr) {
		std::cout << "(";
		for (auto it = i->call->parameters.begin(); it != i->call->parameters.end(); ++it) {
			if (it != i->call->parameters.begin()) {
				std::cout << ", ";
			}
			print_expression(*it);
		}
		std::cout << ")";
	}
}

static void print_fsmenumelement(AST::FSM_Enumeration_Element* e)
{
	std::cout << " [" << e->enum_name << "] " << e->enum_element;
}

static void print_literal(AST::Literal* literal)
{
	if (literal->negation) {
		std::cout << "-";
	}
	std::cout << literal->literal;
}

static void print_ternary(AST::TernaryOperator* t)
{
	std::cout << "if ";
	print_expression(t->cond);
	std::cout << " then ";
	print_expression(t->ifblock);
	std::cout << " else ";
	print_expression(t->elseblock);
	std::cout << "end";
}

static void print_generator(AST::Generator* g)
{
	if (g->type != nullptr) {
		print_type(*g->type);
	}
	std::cout << " " << g->identifier.name << " in ";
	print_expression(g->start);
	std::cout << " to ";
	print_expression(g->end);
}

static void print_listcomprehension(AST::ListComprehension* l)
{
	std::cout << "[ ";
	for (auto it = l->expressions.begin(); it != l->expressions.end(); ++it) {
		if (it != l->expressions.begin()) {
			std::cout << ", ";
		}
		print_expression(*it);
	}

	if (!l->generators.empty()) {
		std::cout << " : ";
		for (auto it = l->generators.begin(); it != l->generators.end(); ++it) {
			if (it != l->generators.begin()) {
				std::cout << ", ";
			}
			print_generator(*it);
		}
	}

	std::cout << " ]";
}

static void print_operator(AST::Operator* o)
{
	decode_baseexpression(o->left);
	std::cout << " " << o->ops << " ";
	decode_baseexpression(o->right);
}

static void print_portpreview(AST::PortPreview* p)
{
	std::cout << "preview(" << p->port << ", ";
	print_expression(p->index->index);
	std::cout << ")";
}

static void print_portsize(AST::PortSize* p)
{
	std::cout << "size(" << p->port << ")";
}

static void print_portfree(AST::PortFree* p)
{
	std::cout << "free(" << p->port << ")";
}

static void decode_baseexpression(AST::BaseExpression* b)
{
	++ast_print_depth;
	if (ast_print_depth > AST_PRINT_MAX_DEPTH) {
		std::cout << "End: max depth reached!";
		return;
	}
	if (b == nullptr) {
		std::cout << "<null>";
		return;
	}
	if (dynamic_cast<AST::Literal*>(b) != nullptr) {
		print_literal(dynamic_cast<AST::Literal*>(b));
	}
	else if (dynamic_cast<AST::Identifier*>(b) != nullptr) {
		print_identifier(dynamic_cast<AST::Identifier*>(b));
	}
	else if (dynamic_cast<AST::TernaryOperator*>(b) != nullptr) {
		print_ternary(dynamic_cast<AST::TernaryOperator*>(b));
	}
	else if (dynamic_cast<AST::ListComprehension*>(b) != nullptr) {
		print_listcomprehension(dynamic_cast<AST::ListComprehension*>(b));
	}
	else if (dynamic_cast<AST::Operator*>(b) != nullptr) {
		print_operator(dynamic_cast<AST::Operator*>(b));
	}
	else if (dynamic_cast<AST::Expression*>(b) != nullptr) {
		print_expression(dynamic_cast<AST::Expression*>(b));
	}
	else if (dynamic_cast<AST::PortPreview*>(b) != nullptr) {
		print_portpreview(dynamic_cast<AST::PortPreview*>(b));
	}
	else if (dynamic_cast<AST::PortSize*>(b) != nullptr) {
		print_portsize(dynamic_cast<AST::PortSize*>(b));
	}
	else if (dynamic_cast<AST::PortFree*>(b) != nullptr) {
		print_portfree(dynamic_cast<AST::PortFree*>(b));
	}
	else if (dynamic_cast<AST::FSM_Enumeration_Element*>(b) != nullptr) {
		print_fsmenumelement(dynamic_cast<AST::FSM_Enumeration_Element*>(b));
	}
	else {
		std::cout << "Unknown";
	}
	--ast_print_depth;
}

static void print_expression(AST::Expression* e)
{
	if (e->brakets) {
		std::cout << "(";
	}
	decode_baseexpression(e->child);
	if (e->brakets) {
		std::cout << ")";
	}
}

void AST::print_statement(
	AST::Statement* s,
	std::string prefix)
{
	++ast_print_depth;
	if (ast_print_depth > AST_PRINT_MAX_DEPTH) {
		std::cout << "End: max depth reached!\n";
		return;
	}
	std::cout << prefix;
	if (dynamic_cast<AST::IfStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::IfStatement*>(s);
		std::cout << "IfStatement : [\n";
		std::cout << prefix << "\tCondition: [ ";
		print_expression(i->condition);
		std::cout << " ],\n";
		std::cout << prefix << "\tIfBlock : [\n";
		for (auto s : i->ifblock) {
			print_statement(s, prefix + "\t\t");
		}
		std::cout << prefix << "\t],\n";
		if (i->elseblock != nullptr) {
			std::cout << prefix << "\tElseBlock : [\n";
			for (auto s : i->elseblock->statements) {
				print_statement(s, prefix + "\t\t");
			}
			std::cout << prefix << "\t],\n";
		}
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::CallStatement*>(s) != nullptr) {
		auto c = dynamic_cast<AST::CallStatement*>(s);
		std::cout << "CallStatement : [\n";
		std::cout << prefix << "\tname: " << c->name.name << ",\n";
		for (auto p : c->parameters) {
			std::cout << prefix << "\tParameter: [ ";
			print_expression(p);
			std::cout << " ],\n";
		}
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::BlockStatement*>(s) != nullptr) {
		auto b = dynamic_cast<AST::BlockStatement*>(s);
		std::cout << "BlockStatement : [\n";
		for (auto v : b->vars) {
			print_vardef(v, prefix + "\t");
		}
		for (auto s : b->statements) {
			print_statement(s, prefix + "\t");
		}
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::WhileStatement*>(s) != nullptr) {
		auto w = dynamic_cast<AST::WhileStatement*>(s);
		std::cout << "WhileStatement : [\n";
		std::cout << prefix << "\tCondition: [ ";
		print_expression(w->condition);
		std::cout << " ],\n";
		for (auto v : w->vars) {
			print_vardef(v, prefix + "\t");
		}
		for (auto s : w->statements) {
			print_statement(s, prefix + "\t");
		}
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::ForeachStatement*>(s) != nullptr) {
		auto f = dynamic_cast<AST::ForeachStatement*>(s);
		std::cout << "ForeachStatement : [\n";
		for (auto g : f->generators) {
			std::cout << prefix << "\tGenerator: [ ";
			print_generator(g);
			std::cout << " ],\n";
		}
		for (auto v : f->vars) {
			print_vardef(v, prefix + "\t");
		}
		for (auto s : f->statements) {
			print_statement(s, prefix + "\t");
		}
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::InputChannelReadStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::InputChannelReadStatement*>(s);
		std::cout << "ChannelRead : [ port: " << i->port.name;
		std::cout << ", val: " << i->identifier.name;
		if (i->index != nullptr) {
			std::cout << ", index: ";
			print_expression(i->index->index);
		}
		std::cout << "],\n";
	}
	else if (dynamic_cast<AST::OutputChannelWriteStatement*>(s) != nullptr) {
		auto o = dynamic_cast<AST::OutputChannelWriteStatement*>(s);
		std::cout << "ChannelWrite : [ port: " << o->port.name;
		std::cout << ", val: ";
		print_expression(o->expr);
		std::cout << "],\n";
	}
	else if (dynamic_cast<AST::AssignmentStatement*>(s) != nullptr) {
		auto a = dynamic_cast<AST::AssignmentStatement*>(s);
		std::cout << "Assignment : [\n";
		std::cout << prefix << "\tlval: [ name: " << a->identifier.name;
		for(auto x : a->indices) {
			std::cout << ", index: ";
			print_expression(x->index);
		}
		std::cout << "],\n";
		if (a->constasgn) {
			std::cout << prefix << "\tconst: true,\n";
		}
		else {
			std::cout << prefix << "\tconst: false,\n";
		}
		std::cout << prefix << "\trval: [ ";
		print_expression(a->asgnvalue);
		std::cout << " ]\n";
		std::cout << prefix << "],\n";
	}
	else if (dynamic_cast<AST::ReturnStatement*>(s) != nullptr) {
		std::cout << "Return\n";
	}
	else {
		std::cout << "Unknown Statement\n";
	}
	--ast_print_depth;
}

static void print_type(AST::Type t)
{
	++ast_print_depth;
	if (ast_print_depth > AST_PRINT_MAX_DEPTH) {
		std::cout << "End: max depth reached!";
		return;
	}
	std::cout << "[" << t.type.name;
	if (t.listtype != nullptr) {
		std::cout << ", listtype : ";
		print_type(*t.listtype);
	}
	if (t.size != nullptr) {
		std::cout << ", size: ";
		print_expression(t.size);
	}
	std::cout << "]";
	--ast_print_depth;
}

static void print_parameter(AST::Parameter* param)
{
	std::cout << "\t\t\tParameter : [ name: " << param->name.name << ", type: ";
	print_type(param->type);
	std::cout << " ],\n";
}

static void print_inputpattern(AST::Input_Pattern* input)
{
	std::cout << "\t\t\tInputpattern : [";
	std::cout << "Port: " << input->port.name << ", ID: [ ";
	for (auto it = input->IDs.begin(); it != input->IDs.end(); ++it) {
		if (it != input->IDs.begin()) {
			std::cout << ", ";
		}
		std::cout << it->name;
	}
	std::cout << " ]";
	if (input->repeat != nullptr) {
		std::cout << ", repeat: ";
		print_expression(input->repeat);
		std::cout << "]";
	}
	std::cout << "],\n";
}

static void print_outputexpression(AST::Output_Expression* output)
{
	std::cout << "\t\t\tOutputexpression : [\n";
	std::cout << "\t\t\t\tPort: " << output->port.name << ",\n";
	for (auto out : output->expressions) {
		std::cout << "\t\t\t\tExpression: [ ";
		print_expression(out);
		std::cout << " ],\n";
	}
	if (output->repeat != nullptr) {
		std::cout << "\t\t\t\trepeat: [";
		print_expression(output->repeat);
		std::cout << "]\n";
	}
	std::cout << "\t\t\t],\n";
}

static void print_action(
	AST::Action* action,
	bool init = false)
{
	if (init) {
		std::cout << "\t\tInitaction : [\n";
	}
	else {
		std::cout << "\t\tAction : [\n";
		std::cout << "\t\t\tname: " << action->name.name << ",\n";
	}
	for (auto in : action->input_patterns) {
		print_inputpattern(in);
	}
	for (auto out : action->output_expressions) {
		print_outputexpression(out);
	}
	for (auto guard : action->guards) {
		std::cout << "\t\t\tGuard : [ ";
		print_expression(guard);
		std::cout << " ],\n";
	}
	for (auto guard : action->output_guards) {
		std::cout << "\t\t\tOutputGuard : [ ";
		print_expression(guard);
		std::cout << " ],\n";
	}
	for (auto var : action->vars) {
		print_vardef(var, "\t\t\t");
	}
	if (!action->statements.empty()) {
		std::cout << "\t\t\tStatements : [\n";
		for (auto s : action->statements) {
			print_statement(s, "\t\t\t\t");
		}
		std::cout << "\t\t\t]\n";
	}
	std::cout << "\t\t]\n";
}

static void print_fsm(AST::Schedule_FSM* fsm)
{
	std::cout << "\t\tSchedule FSM : [\n";
	std::cout << "\t\t\tinitial state: " << fsm->inital_state.name << ",\n";
	for (auto state : fsm->transitions) {
		std::cout << "\t\t\tTransition : [ State: " << state->state.name << ", Actions : [ ";
		for (auto a : state->actions) {
			std::cout << a.name << " ";
		}
		std::cout << "], next: " << state->next.name << "]\n";
	}
	std::cout << "\t\t]\n";
}

static void print_priority(AST::Action_Priority* prio)
{
	std::cout << "\t\t\tPriority : [ ";

	for (auto it = prio->prio_rel.begin(); it != prio->prio_rel.end(); ++it) {
		if (it != prio->prio_rel.begin()) {
			std::cout << " > ";
		}
		std::cout << it->name;
	}

	std::cout << " ]\n";
}

static void print_function(AST::Function* function)
{
	std::cout << "\t\tFunction : [\n";
	std::cout << "\t\t\tname: " << function->name.name << ",\n";
	std::cout << "\t\t\treturn type : ";
	print_type(function->ret_type);
	std::cout << ",\n";
	for (auto param : function->parameters) {
		print_parameter(param);
	}
	std::cout << "\t\t\tExpression: ";
	print_expression(function->expression);
	std::cout << "\n\t\t]\n";
}

static void print_nativefunction(AST::NativeFunction* native)
{
	std::cout << "\t\tNative Function : [\n";
	std::cout << "\t\t\tname: " << native->name.name << ",\n";
	std::cout << "\t\t\treturn type: ";
	print_type(native->ret_type);
	std::cout << ",\n";
	for (auto param : native->parameters) {
		print_parameter(param);
	}
	std::cout << "\t\t]\n";
}

static void print_nativeprocedure(AST::NativeProcedure* native)
{
	std::cout << "\t\tNative Procedure : [\n";
	std::cout << "\t\t\tname: " << native->name.name << ",\n";
	for (auto param : native->parameters) {
		print_parameter(param);
	}
	std::cout << "\t\t]\n";
}

static void print_procedure(AST::Procedure* procedure)
{
	std::cout << "\t\tProcedure : [\n";
	std::cout << "\t\t\tname: " << procedure->name.name << ",\n";
	for (auto param : procedure->parameters) {
		print_parameter(param);
	}
	for (auto var : procedure->vars) {
		print_vardef(var, "\t\t\t");
	}
	std::cout << "\t\t\tStatements : [\n";
	for (auto s : procedure->statements) {
		print_statement(s, "\t\t\t\t");
	}
	std::cout << "\t\t\t]\n";
	std::cout << "\t\t]\n";
}

static void print_vardef(
	AST::VarDefinition* vardef,
	std::string prefix = "\t\t")
{
	std::cout << prefix << "Variable Definition : [ Type: ";
	print_type(vardef->type);
	std::cout << ", name: " << vardef->name.name;
	for(auto a : vardef->arrays) {
		std::cout << ", array: ";
		print_expression(a);
	}
	if (vardef->constassign) {
		std::cout << ", const: true";
	}
	else {
		std::cout << ", const: false";
	}
	if (vardef->assign != nullptr) {
		std::cout << ", Value: ";
		print_expression(vardef->assign);
	}
	std::cout << "]\n";
}

static void print_actorparam(AST::ActorParameter* param)
{
	std::cout << "\t\tParameter : [ Type: ";
	print_type(param->type);
	std::cout << ", name: " << param->name.name;
	if (param->asign != nullptr) {
		std::cout << ", Default: ";
		print_expression(param->asign);
	}
	std::cout << "]\n";
}

static void print_inport(AST::Port* port)
{
	std::cout << "\t\tInport : [ Type: ";
	print_type(port->type);
	std::cout << ", name: " << port->name.name << "]\n";
}

static void print_outport(AST::Port* port)
{
	std::cout << "\t\tOutport : [ Type: ";
	print_type(port->type);
	std::cout << ", name: " << port->name.name << "]\n";
}

static void print_fsm_enum(AST::FSM_Enumeration* fsmenum)
{
	std::cout << "\t\tFSM Enum: [ name: " << fsmenum->name << ", initial state: " << fsmenum->inital_state << ", states: [";
	for (auto it = fsmenum->states.begin(); it != fsmenum->states.end(); ++it) {
		if (it != fsmenum->states.begin()) {
			std::cout << ", ";
		}
		std::cout << *it;
	}
	std::cout << "]]\n";
}

static void print_actor(AST::Actor* actor)
{
	std::cout << "\tActor : [\n";


	for (auto param : actor->parameters) {
		print_actorparam(param);
	}
	for (auto inport : actor->inports) {
		print_inport(inport);
	}
	for (auto outport : actor->outports) {
		print_outport(outport);
	}
	for (auto vardef : actor->vars) {
		print_vardef(vardef);
	}
	for (auto native : actor->nativefunctions) {
		print_nativefunction(native);
	}
	for (auto native : actor->nativeprocedures) {
		print_nativeprocedure(native);
	}
	for (auto function : actor->functions) {
		print_function(function);
	}
	for (auto procedure : actor->procedures) {
		print_procedure(procedure);
	}
	for (auto action : actor->actions) {
		print_action(action);
	}
	if (actor->init != nullptr) {
		print_action(actor->init, true);
	}
	if (actor->fsm != nullptr) {
		print_fsm(actor->fsm);
	}
	if (!actor->prios.empty()) {
		std::cout << "\t\tPriorities: [\n";
		for (auto prio : actor->prios) {
			print_priority(prio);
		}
		std::cout << "\t\t]\n";
	}
	for (auto fsmenum : actor->fsm_enums) {
		print_fsm_enum(fsmenum);
	}

	std::cout << "\t]\n";
}

static void print_unit(AST::Unit* unit)
{
	std::cout << "\tUnit : [\n";

	for (auto procedure : unit->procedures) {
		print_procedure(procedure);
	}
	for (auto function : unit->functions) {
		print_function(function);
	}
	for (auto native : unit->nativefunctions) {
		print_nativefunction(native);
	}
	for (auto native : unit->nativeprocedures) {
		print_nativeprocedure(native);
	}
	for (auto vardef : unit->vars) {
		print_vardef(vardef);
	}

	std::cout << "\t]\n";
}

void AST::print_ast(AST::AST_Root* ast)
{
	std::cout << "File : [\n";

	for (auto import : ast->imports) {
		std::cout << "\tImport : [ Path: " << import->path.name << " Symbol: " << import->symbol.name << " ]\n";
	}

	if (ast->actor != nullptr) {
		print_actor(ast->actor);
	}
	if (ast->unit != nullptr) {
		print_unit(ast->unit);
	}

	std::cout << "];" << std::endl;
}