#include "AST_Transformations.hpp"

#include <assert.h>
#include <set>
#include <map>
#include <tuple>
#include <algorithm>

/*
	Function object to sort actions according to their priority defined in the priority block of the actor.
*/
class Prio_Comparison_Object {
	std::vector<AST::Action_Priority*>& priorities;

	unsigned get_prio_value(const std::string& name) const {
		unsigned index = 0;
		for (auto it = priorities.begin(); it != priorities.end(); ++it) {
			for (auto p : (*it)->prio_rel) {
				if ((p.name.size() <= name.size()) && (name.substr(0, p.name.size()) == p.name)) {
					return index;
				}
				++index;
			}
		}
		return index;
	}

public:
	Prio_Comparison_Object(std::vector<AST::Action_Priority*>& _priorities) :priorities(_priorities) {}

	/* ensures strict weak-ordering as required by std::sort */
	bool operator()(AST::Action* action1, AST::Action* action2) const {
		return get_prio_value(action1->name.name) < get_prio_value(action2->name.name);
	}
};

static void sort_actions(
	std::vector<AST::Action_Priority*>& prios,
	std::vector<AST::Action*>& actions,
	std::vector<AST::Action*>& sorted_actions)
{
	if (prios.empty() || (actions.size() == 1)) {
		sorted_actions.insert(sorted_actions.end(), actions.begin(), actions.end());
	}
	else {
		std::vector<AST::Action*> tmp(actions.begin(), actions.end());
		std::sort(tmp.begin(), tmp.end(), Prio_Comparison_Object{ prios });
		sorted_actions.swap(tmp);
	}
}

AST::Statement* AST_Transform::guardprio_cond(
	AST::AST_Root* ast,
	std::vector<AST::Action*> actions,
	std::map<std::string, std::vector<AST::Statement*>>& additional_code,
	std::map<std::string, std::string>& action_function,
	bool add_else_break)
{
	assert(ast != nullptr);
	assert(ast->actor != nullptr);
	AST::Statement* new_action = nullptr;
	/* Determine action order */
	std::vector<AST::Action*> ordered_actions;
	sort_actions(ast->actor->prios, actions, ordered_actions);

	AST::IfStatement* prev_if = nullptr;

	/* Foreach action in order */
	unsigned oa_index = 0;
	for (auto a : ordered_actions) {
		/* -> transform guard to if cond */
		AST::Expression* cond;
		if (a->guards.empty()) {
			cond = new AST::Expression{};
			AST::Literal* l = new AST::Literal{};
			l->literal = "true";
			cond->child = l;
		}
		else if (a->guards.size() == 1) {
			cond = a->guards.front()->clone();
		}
		else {
			assert(a->guards.size() > 1);
			cond = new AST::Expression{};
			AST::Operator* op = new AST::Operator{};
			op->ops = "&&";
			cond->child = op;
			unsigned processed = 0;
			for (auto g : a->guards) {
				AST::Expression* expr = g->clone();
				processed++;
				expr->brakets = true;
				if (op->left == nullptr) {
					op->left = expr;
				}
				else {
					if (processed == a->guards.size()) {
						op->right = expr;
					}
					else {
						AST::Operator* n = new AST::Operator{};
						op->right = n;
						op = n;
						n->left = expr;
						n->ops = "&&";
					}
				}
			}
		}

		AST::IfStatement* freecheck_if = nullptr;
		if (!a->output_guards.empty()) {
			freecheck_if = new AST::IfStatement{};
			AST::Expression* freecond = new AST::Expression{};
			freecheck_if->condition = freecond;
			if (a->output_guards.size() == 1) {
				auto g = a->output_guards.front()->clone();
				freecond->child = g;
			}
			else {
				AST::Operator* op = new AST::Operator{};
				op->ops = "&&";
				freecond->child = op;
				unsigned processed = 0;
				for (auto g : a->output_guards) {
					AST::Expression* expr = g->clone();
					processed++;
					expr->brakets = true;
					if (op->left == nullptr) {
						op->left = expr;
					}
					else {
						if (processed == a->output_guards.size()) {
							op->right = expr;
						}
						else {
							AST::Operator* n = new AST::Operator{};
							op->right = n;
							op = n;
							n->left = expr;
							n->ops = "&&";
						}
					}
				}
			}
		}

		if (a->guards.empty() && (oa_index == (ordered_actions.size() - 1)) && !add_else_break) {
			AST::BlockStatement* block = new AST::BlockStatement{};
			AST::CallStatement* call = new AST::CallStatement{};
			call->name.name = action_function[a->name.name];
			block->statements.push_back(call);
			if (additional_code.contains(a->name.name)) {
				block->statements.insert(block->statements.end(), additional_code[a->name.name].begin(), additional_code[a->name.name].end());
			}
			if (prev_if != nullptr) {
				AST::ElseStatement* e = new AST::ElseStatement{};
				if (freecheck_if != nullptr) {
					freecheck_if->ifblock.push_back(block);
					e->statements.push_back(freecheck_if);
				}
				else {
					e->statements.push_back(block);
				}
				prev_if->elseblock = e;
			}
			else {
				assert(new_action == nullptr);
				new_action = block;
			}
		}
		else {
			/* -> create if statement with previously created cond and statements of action */
			AST::IfStatement* cur = new AST::IfStatement{};
			cur->condition = cond;
			AST::BlockStatement* block = new AST::BlockStatement{};
			AST::CallStatement* call = new AST::CallStatement{};
			call->name.name = action_function[a->name.name];
			block->statements.push_back(call);
			if (additional_code.contains(a->name.name)) {
				block->statements.insert(block->statements.end(), additional_code[a->name.name].begin(), additional_code[a->name.name].end());
			}
			if (freecheck_if != nullptr) {
				freecheck_if->ifblock.push_back(block);
				cur->ifblock.push_back(freecheck_if);
			}
			else {
				cur->ifblock.push_back(block);
			}

			/* -> add to new_action body */
			if (prev_if == nullptr) {
				assert(new_action == nullptr);
				new_action = cur;
			}
			else {
				AST::ElseStatement* e = new AST::ElseStatement{};
				e->statements.push_back(cur);
				prev_if->elseblock = e;
			}
			prev_if = cur;
		}

		oa_index++;
	}

	if (add_else_break && (prev_if != nullptr)) {
		AST::ElseStatement* e = new AST::ElseStatement{};
		prev_if->elseblock = e;
		AST::TerminateLoopStatement* t = new AST::TerminateLoopStatement{};
		e->statements.push_back(t);
	}

	return new_action;
}

struct replacement_data {
	AST::Expression* index = nullptr;
	AST::Expression* repeat = nullptr;
	std::string port = "";
	unsigned offset = 0;
	unsigned overall_sz = 0;
};

static AST::PortPreview* create_preview(
	replacement_data& d,
	std::string prev)
{
	AST::PortPreview* p = new AST::PortPreview{};
	p->port = d.port;
	p->prev_identifier = prev;
	p->index = new AST::Index{};
	AST::Expression* e = new AST::Expression{};

	if (d.repeat != nullptr) {
		assert(d.index != nullptr);
		AST::Operator* mult = new AST::Operator{};
		AST::Operator* add = new AST::Operator{};
		mult->ops = "*";
		add->ops = "+";
		AST::Literal* l = new AST::Literal{};
		add->right = l;
		add->left = mult;
		e->child = add;
		mult->left = new AST::Expression{ *d.index };
		AST::Literal* l2 = new AST::Literal{};
		l2->literal = std::to_string(d.overall_sz);
		mult->right = l2;
		l->literal = std::to_string(d.offset);
	}
	else {
		AST::Literal* l = new AST::Literal{};
		l->literal = std::to_string(d.offset);
		e->child = l;
	}

	p->index->index = e;

	return p;
}

static void add_portpreview(
	AST::BaseExpression* expr,
	std::map<std::string, replacement_data>& id_preview)
{
	assert(expr != nullptr);
	if (dynamic_cast<AST::Expression*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Expression*>(expr);
		/* check if child is identifier, otherwise continue */
		if (dynamic_cast<AST::Identifier*>(tmp->child) != nullptr) {
			auto i = dynamic_cast<AST::Identifier*>(tmp->child);
			if (id_preview.contains(i->identifier)) {
				replacement_data d = id_preview[i->identifier];
				assert(i->indices.size() <= 1);
				for (auto index : i->indices) {
					d.index = index->index;
				}
				auto p = create_preview(d, i->identifier);
				delete i;
				tmp->child = p;
			}
		}
		else {
			add_portpreview(tmp->child, id_preview);
		}
	}
	else if (dynamic_cast<AST::Operator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Operator*>(expr);
		/* check left and right if identifier, otherwise continue */
		if (dynamic_cast<AST::Identifier*>(tmp->left) != nullptr) {
			auto i = dynamic_cast<AST::Identifier*>(tmp->left);
			if (id_preview.contains(i->identifier)) {
				replacement_data d = id_preview[i->identifier];
				assert(i->indices.size() <= 1);
				for (auto index : i->indices) {
					d.index = index->index;
				}
				auto p = create_preview(d, i->identifier);
				delete i;
				tmp->left = p;
			}
		}
		else {
			add_portpreview(tmp->left, id_preview);
		}
		if (dynamic_cast<AST::Identifier*>(tmp->right) != nullptr) {
			auto i = dynamic_cast<AST::Identifier*>(tmp->right);
			if (id_preview.contains(i->identifier)) {
				replacement_data d = id_preview[i->identifier];
				assert(i->indices.size() <= 1);
				for (auto index : i->indices) {
					d.index = index->index;
				}
				auto p = create_preview(d, i->identifier);
				delete i;
				tmp->right = p;
			}
		}
		else {
			add_portpreview(tmp->right, id_preview);
		}
	}
	else if (dynamic_cast<AST::TernaryOperator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::TernaryOperator*>(expr);
		add_portpreview(tmp->cond, id_preview);
		add_portpreview(tmp->ifblock, id_preview);
		add_portpreview(tmp->elseblock, id_preview);
	}
	else if (dynamic_cast<AST::Literal*>(expr) != nullptr) {
		/* empty */
	}
	else if (dynamic_cast<AST::ListComprehension*>(expr) != nullptr) {
		/* empty - I assume that this is not part of guard here, let's see */
	}
	else if (dynamic_cast<AST::Identifier*>(expr) != nullptr) {
		/* empty - handled in parent */
	}
	else {
		assert((dynamic_cast<AST::PortPreview*>(expr) != nullptr) || (dynamic_cast<AST::PortSize*>(expr) != nullptr));
	}
}

static std::string find_unused_name(
	std::string name,
	std::set<std::string>& used_symbols)
{
	std::string current = name + "_";
	unsigned index = 0;
	current += std::to_string(index);
	while (used_symbols.contains(current)) {
		++index;
		current = name + "_" + std::to_string(index);
	}
	return current;
}

AST::AST_Root* AST_Transform::inputpattern_channelreadstmt(
	AST::AST_Root* ast,
	std::set<std::string>& used_symbols)
{
	assert(ast != nullptr);
	assert(ast->actor != nullptr);

	std::map<std::string, AST::Type*> port_type;
	for (auto port : ast->actor->inports) {
		port_type[port->name.name] = &port->type;
	}

	std::vector<AST::Action*> actions;
	actions.insert(actions.begin(), ast->actor->actions.begin(), ast->actor->actions.end());
	if (ast->actor->init != nullptr) {
		actions.push_back(ast->actor->init);
	}

	for (auto action : actions) {
		std::vector<AST::Statement*> pre_stmt;
		std::vector<AST::VarDefinition*> pre_vardef;
		std::map<std::string, replacement_data> id_port;
		for (auto input : action->input_patterns) {
			AST::PortSize* portsize = new AST::PortSize{};
			portsize->port = input->port.name;
			AST::Expression* sizecheck = new AST::Expression{};
			AST::Operator* checkops = new AST::Operator{};
			checkops->ops = ">=";
			checkops->left = portsize;
			sizecheck->child = checkops;

			if (input->repeat != nullptr) {
				AST::ForeachStatement* forloop = new AST::ForeachStatement{};
				AST::Generator* gen = new AST::Generator{};
				forloop->generators.push_back(gen);
				gen->type = new AST::Type{};
				gen->type->type.name = "int";
				gen->identifier.name = find_unused_name("i", used_symbols);
				AST::Expression* gen_start = new AST::Expression{};
				AST::Expression* gen_end = new AST::Expression{};
				gen->start = gen_start;
				gen->end = gen_end;
				AST::Literal* l = new AST::Literal{};
				l->literal = "0";
				gen_start->child = l;
				AST::Operator* op = new AST::Operator{};
				gen_end->child = op;
				op->ops = "-";
				AST::Expression* rr = new AST::Expression{ *input->repeat };
				rr->brakets = true;
				op->left = rr;
				l = new AST::Literal{};
				l->literal = "1";
				op->right = l;
				
				unsigned offset = 0;
				for (auto in : input->IDs) {
					AST::VarDefinition* v = new AST::VarDefinition{};
					v->name = in;
					v->constassign = false;
					v->type = *port_type[input->port.name];
					AST::Expression* r = new AST::Expression{ *input->repeat };
					v->arrays.push_back(r);
					pre_vardef.push_back(v);
					AST::InputChannelReadStatement* read = new AST::InputChannelReadStatement{};
					forloop->statements.push_back(read);
					read->identifier = in;
					AST::Index* index = new AST::Index{};
					index->index = new AST::Expression{};
					AST::Identifier* q = new AST::Identifier{};
					q->identifier = gen->identifier.name;
					index->index->child = q;
					read->index = index;
					read->port = input->port;

					replacement_data d;
					d.port = input->port.name;
					d.repeat = input->repeat;
					d.offset = offset;
					d.index = nullptr;
					d.overall_sz = static_cast<unsigned>(input->IDs.size());
					id_port[in.name] = d;
					
					read->size = static_cast<unsigned>(input->IDs.size());
					read->offset = offset;
					++offset;
				}
				pre_stmt.push_back(forloop);

				AST::Expression* opsright = new AST::Expression{};
				checkops->right = opsright;
				opsright->brakets = true;
				AST::Operator* multops = new AST::Operator{};
				multops->ops = "*";
				AST::Literal* sizelit = new AST::Literal{};
				sizelit->literal = std::to_string(input->IDs.size());
				multops->right = sizelit;
				AST::Expression* rrr = new AST::Expression{ *input->repeat };
				rrr->brakets = true;
				multops->left = rrr;
				opsright->child = multops;
			}
			else {
				AST::Literal* sizelit = new AST::Literal{};
				sizelit->literal = std::to_string(input->IDs.size());
				checkops->right = sizelit;

				unsigned offset = 0;
				for (auto in : input->IDs) {
					AST::VarDefinition* v = new AST::VarDefinition{};
					v->name = in;
					v->constassign = false;
					v->type = *port_type[input->port.name];
					pre_vardef.push_back(v);

					replacement_data d;
					d.repeat = nullptr;
					d.port = input->port.name;
					d.index = nullptr;
					d.offset = offset;
					d.overall_sz = static_cast<unsigned>(input->IDs.size());
					id_port[in.name] = d;
					
					
					AST::InputChannelReadStatement* read = new AST::InputChannelReadStatement{};
					pre_stmt.push_back(read);
					read->identifier = in;
					read->port = input->port;
					read->offset = offset;
					read->size = static_cast<unsigned>(input->IDs.size());
					++offset;
				}
			}
			action->guards.push_back(sizecheck);
		}

		for (auto g : action->guards) {
			add_portpreview(g, id_port);
		}

		pre_stmt.insert(pre_stmt.end(), action->statements.begin(), action->statements.end());
		action->statements = pre_stmt;
		action->vars.insert(action->vars.end(), pre_vardef.begin(), pre_vardef.end());
		action->input_patterns.clear();
	}

	return ast;
}

static AST::Type* find_type_outport(
	AST::AST_Root* ast,
	std::string port)
{
	for (auto p : ast->actor->outports) {
		if (p->name.name == port) {
			return new AST::Type{ p->type };
		}
	}
	assert(0);
	return nullptr;
}

static unsigned tmpvarcount = 0;

AST::AST_Root* AST_Transform::outputexpr_channelwritestmt(
	AST::AST_Root* ast,
	std::set<std::string>& used_symbols)
{
	assert(ast != nullptr);
	assert(ast->actor != nullptr);

	std::vector<AST::Action*> actions;
	actions.insert(actions.begin(), ast->actor->actions.begin(), ast->actor->actions.end());
	if (ast->actor->init != nullptr) {
		actions.push_back(ast->actor->init);
	}

	for (auto action : actions) {
		for (auto out : action->output_expressions) {
			AST::Expression* freecheck = new AST::Expression{};
			AST::PortFree* portfree = new AST::PortFree{};
			portfree->port = out->port.name;
			AST::Operator* compare = new AST::Operator{};
			compare->ops = ">=";
			compare->left = portfree;
			freecheck->child = compare;

			if (out->repeat != nullptr) {
				AST::ForeachStatement* forloop = new AST::ForeachStatement{};
				AST::Generator* gen = new AST::Generator{};
				forloop->generators.push_back(gen);
				gen->type = new AST::Type{};
				gen->type->type.name = "int";
				gen->identifier.name = find_unused_name("i", used_symbols);
				AST::Expression* gen_start = new AST::Expression{};
				AST::Expression* gen_end = new AST::Expression{};
				gen->start = gen_start;
				gen->end = gen_end;
				AST::Literal* l = new AST::Literal{};
				l->literal = "0";
				gen_start->child = l;
				AST::Operator* op = new AST::Operator{};
				gen_end->child = op;
				op->ops = "-";
				AST::Expression* left = new AST::Expression{ *out->repeat };
				op->left = left;
				left->brakets = true;
				l = new AST::Literal{};
				l->literal = "1";
				op->right = l;

				for (auto v : out->expressions) {
					/* If it is a ListComprehension, let's assume it produces "repeat" many tokens */
					if (dynamic_cast<AST::ListComprehension*>(v->child) != nullptr) {
						auto list = dynamic_cast<AST::ListComprehension*>(v->child);
						AST::ForeachStatement* comp_loop = new AST::ForeachStatement{};
						AST::VarDefinition* var = new AST::VarDefinition{};
						std::string t = "tmp$" + std::to_string(tmpvarcount++);
						var->name.name = t;
						var->constassign = false;
						var->type.type.name = "list";
						var->type.listtype = new AST::Type{};
						var->type.listtype = find_type_outport(ast, out->port.name);
						var->type.size = new AST::Expression{ *out->repeat };
						action->vars.push_back(var);
						var = new AST::VarDefinition{};
						std::string t2 = "tmp$" + std::to_string(tmpvarcount++);
						var->name.name = t2;
						var->type.type.name = "int";
						var->constassign = false;
						var->assign = new AST::Expression{};
						AST::Literal* l = new AST::Literal{};
						l->literal = "0";
						var->assign->child = l;
						action->vars.push_back(var);

						for (auto g : list->generators) {
							comp_loop->generators.push_back(g);
						}
						for (auto e : list->expressions) {
							AST::AssignmentStatement* as = new AST::AssignmentStatement{};
							comp_loop->statements.push_back(as);
							as->identifier.name = t;
							AST::Index *q = new AST::Index{};
							as->indices.push_back(q);
							q->index = new AST::Expression{};
							AST::Identifier* j = new AST::Identifier{};
							j->identifier = t2;
							j->unary_right = new AST::UnaryOperator{};
							j->unary_right->ops = "++";
							q->index->child = j;
							as->asgnvalue = e;
							as->constasgn = false;

							AST::OutputChannelWriteStatement* write = new AST::OutputChannelWriteStatement{};
							write->port = out->port;
							write->expr = new AST::Expression{};
							AST::Identifier* x = new AST::Identifier{};
							x->identifier = t;
							AST::Index* b = new AST::Index{};
							x->indices.push_back(b);
							write->expr->child = x;
							b->index = new AST::Expression{};
							AST::Identifier* y = new AST::Identifier{};
							y->identifier = gen->identifier.name;
							b->index->child = y;
							forloop->statements.push_back(write);
						}
						action->statements.push_back(comp_loop);
						delete list;
					}
					else if (dynamic_cast<AST::Identifier*>(v->child) != nullptr) {
						auto tmp = dynamic_cast<AST::Identifier*>(v->child);
						assert(tmp->indices.empty());
						AST::OutputChannelWriteStatement* write = new AST::OutputChannelWriteStatement{};
						AST::Index* index = new AST::Index{};
						AST::Expression* exprindex = new AST::Expression{};
						index->index = exprindex;
						write->port = out->port;
						write->expr = v;
						AST::Identifier* q = new AST::Identifier{};
						q->identifier = gen->identifier.name;
						exprindex->child = q;
						tmp->indices.push_back(index);

						forloop->statements.push_back(write);
					}
					else {
						AST::OutputChannelWriteStatement* write = new AST::OutputChannelWriteStatement{};
						write->expr = v;
						write->port = out->port;

						forloop->statements.push_back(write);
					}
				}
				action->statements.push_back(forloop);

				AST::Expression* right = new AST::Expression{};
				right->brakets = true;
				compare->right = right;
				AST::Operator* mult = new AST::Operator{};
				mult->ops = "*";
				AST::Literal* lit = new AST::Literal{};
				lit->literal = std::to_string(out->expressions.size());
				mult->right = lit;
				AST::Expression* rr = new AST::Expression{ *out->repeat };
				rr->brakets = true;
				mult->left = rr;
				right->child = mult;

				delete out->repeat;
			}
			else {
				AST::Literal* lit = new AST::Literal{};
				lit->literal = std::to_string(out->expressions.size());
				compare->right = lit;
				for (auto v : out->expressions) {
					AST::OutputChannelWriteStatement* write = new AST::OutputChannelWriteStatement{};
					write->expr = v;
					write->port = out->port;
					action->statements.push_back(write);
				}
			}

			action->output_guards.push_back(freecheck);
		}

		for (auto o : action->output_expressions) {
			delete o;
		}
		action->output_expressions.clear();
	}

	return ast;
}

static void replace_statement(
	std::vector<AST::Statement*>* statements,
	AST::Statement* to_replace,
	AST::Statement* replacement,
	std::vector<AST::Statement*> add)
{
	auto pos = std::find(statements->begin(), statements->end(), to_replace);

	if (pos == statements->end()) {
		return;
	}
	pos = statements->erase(pos);
	if (!add.empty()) {
		pos = statements->insert(pos, add.begin(), add.end());
	}
	pos = statements->insert(pos, replacement);
}

static void replace_preview_by_var(
	AST::BaseExpression* expr,
	std::string port,
	std::string var,
	std::string var_index,
	unsigned var_sz)
{
	assert(expr != nullptr);
	if (dynamic_cast<AST::PortPreview*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::PortPreview*>(expr);
		/* handled by parent */
	}
	else if (dynamic_cast<AST::Expression*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Expression*>(expr);
		if (dynamic_cast<AST::PortPreview*>(tmp->child) != nullptr) {
			auto p = dynamic_cast<AST::PortPreview*>(tmp->child);
			if (p->port == port) {
				AST::Identifier* i = new AST::Identifier{};
				tmp->child = i;
				i->identifier = var;
				if (var_sz > 1) {
					AST::Index* index = new AST::Index{};
					i->indices.push_back(index);
					AST::Operator* mod = new AST::Operator{};
					mod->ops = "%";
					AST::Operator* add = new AST::Operator{};
					add->ops = "+";
					AST::Identifier* vari = new AST::Identifier{};
					vari->identifier = var_index;
					AST::Literal* l = new AST::Literal{};
					l->literal = std::to_string(var_sz);
					mod->right = l;
					mod->left = add;
					add->right = vari;
					add->left = p->index->index;
					p->index->index->brakets = true;

					index->index = new AST::Expression{};
					index->index->child = mod;
				}
				delete p;
			}
		}
		else {
			replace_preview_by_var(tmp->child, port, var, var_index, var_sz);
		}
	}
	else if (dynamic_cast<AST::Operator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Operator*>(expr);
		/* check left and right if PortPreview, otherwise continue */
		if (dynamic_cast<AST::PortPreview*>(tmp->left) != nullptr) {
			auto p = dynamic_cast<AST::PortPreview*>(tmp->left);
			if (p->port == port) {
				AST::Identifier* i = new AST::Identifier{};
				tmp->left = i;
				i->identifier = var;
				if (var_sz > 1) {
					AST::Index* index = new AST::Index{};
					i->indices.push_back(index);
					AST::Operator* mod = new AST::Operator{};
					mod->ops = "%";
					AST::Operator* add = new AST::Operator{};
					add->ops = "+";
					AST::Identifier* vari = new AST::Identifier{};
					vari->identifier = var_index;
					AST::Literal* l = new AST::Literal{};
					l->literal = std::to_string(var_sz);
					mod->right = l;
					mod->left = add;
					add->right = vari;
					add->left = p->index->index;
					p->index->index->brakets = true;

					index->index = new AST::Expression{};
					index->index->child = mod;
				}
				delete p;
			}
		}
		else {
			replace_preview_by_var(tmp->left, port, var, var_index, var_sz);
		}
		if (dynamic_cast<AST::PortPreview*>(tmp->right) != nullptr) {
			auto p = dynamic_cast<AST::PortPreview*>(tmp->right);
			if (p->port == port) {
				AST::Identifier* i = new AST::Identifier{};
				tmp->right = i;
				i->identifier = var;
				if (var_sz > 1) {
					AST::Index* index = new AST::Index{};
					i->indices.push_back(index);
					AST::Operator* mod = new AST::Operator{};
					mod->ops = "%";
					AST::Operator* add = new AST::Operator{};
					add->ops = "+";
					AST::Identifier* vari = new AST::Identifier{};
					vari->identifier = var_index;
					AST::Literal* l = new AST::Literal{};
					l->literal = std::to_string(var_sz);
					mod->right = l;
					mod->left = add;
					add->right = vari;
					add->left = p->index->index;
					p->index->index->brakets = true;

					index->index = new AST::Expression{};
					index->index->child = mod;
				}
				delete p;
			}
		}
		else {
			replace_preview_by_var(tmp->right, port, var, var_index, var_sz);
		}
	}
	else if (dynamic_cast<AST::TernaryOperator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::TernaryOperator*>(expr);
		replace_preview_by_var(tmp->cond, port, var, var_index, var_sz);
		replace_preview_by_var(tmp->ifblock, port, var, var_index, var_sz);
		replace_preview_by_var(tmp->elseblock, port, var, var_index, var_sz);
	}
	else if (dynamic_cast<AST::Literal*>(expr) != nullptr) {
		/* empty */
	}
	else if (dynamic_cast<AST::FSM_Enumeration_Element*>(expr) != nullptr) {
		/* empty */
	}
	else if (dynamic_cast<AST::ListComprehension*>(expr) != nullptr) {
		/* empty - I assume that this is not part of guard here, let's see */
	}
	else if (dynamic_cast<AST::Identifier*>(expr) != nullptr) {
		/* empty - can only be leaf */
	}
	else {
		assert(dynamic_cast<AST::PortSize*>(expr) != nullptr);
	}

}

void AST_Transform::channelreadstmt_assignment(
	AST::Action* action,
	AST::InputChannelReadStatement* read,
	std::string variable,
	std::string variable_index,
	std::string variable_sz,
	unsigned channel_size,
	std::vector<AST::Statement*>* stmt_list)
{
	assert(read != nullptr);
	assert(action != nullptr);

	AST::AssignmentStatement* asgn = new AST::AssignmentStatement{};
	asgn->identifier = read->identifier;
	if (read->index != nullptr) {
		asgn->indices.push_back(read->index);
	}
	AST::Expression* e = new AST::Expression{};
	asgn->asgnvalue = e;
	asgn->constasgn = false;

	AST::Identifier* ident = new AST::Identifier{};
	ident->identifier = variable;
	e->child = ident;

	std::vector<AST::Statement*> follow;
	
	if (channel_size > 1) {
		AST::Index* index = new AST::Index{};
		ident->indices.push_back(index);
		AST::Expression* index_expr = new AST::Expression{};
		AST::Identifier* index_ident = new AST::Identifier{};
		index->index = index_expr;
		index_expr->child = index_ident;
		index_ident->identifier = variable_index;

		/* Now increment index */
		AST::AssignmentStatement* o = new AST::AssignmentStatement{};
		AST::Literal* sz_lit = new AST::Literal{};
		sz_lit->literal = std::to_string(channel_size);
		AST::Identifier* i = new AST::Identifier{};
		AST::Operator* mod = new AST::Operator{};
		mod->ops = "%";
		mod->right = sz_lit;
		mod->left = i;
		AST::UnaryOperator* unop = new AST::UnaryOperator{};
		unop->ops = "++";
		i->unary_left = unop;
		i->identifier = variable_index;
		o->identifier.name = variable_index;
		AST::Expression* asn = new AST::Expression{};
		asn->child = mod;
		o->asgnvalue = asn;
		o->constasgn = false;
		follow.push_back(o);
	}

	/* Update size */
	AST::Identifier* x = new AST::Identifier{};
	AST::Literal* l1 = new AST::Literal{};
	l1->literal = std::to_string(1);
	x->identifier = variable_sz;
	AST::Operator* ops3 = new AST::Operator{};
	ops3->ops = "-";
	ops3->left = x;
	ops3->right = l1;
	AST::AssignmentStatement* o = new AST::AssignmentStatement{};
	o->identifier.name = variable_sz;
	AST::Expression* y = new AST::Expression{};
	y->child = ops3;
	o->asgnvalue = y;
	o->constasgn = false;
	follow.push_back(o);

	replace_statement(stmt_list, read, asgn, follow);

	for (auto g : action->guards) {
		replace_preview_by_var(g, read->port.name, variable, variable_index, channel_size);
	}

	delete read;
}

//FIXME: In certain scenarions variable_index can just be set to zero again.... avoids expensive modulo
void AST_Transform::channelwritestmt_assignment(
	AST::Action* action,
	AST::OutputChannelWriteStatement* write,
	std::string variable,
	std::string variable_index,
	std::string variable_sz,
	unsigned channel_size,
	std::vector<AST::Statement*>* stmt_list)
{
	assert(action != nullptr);
	assert(write != nullptr);

	AST::AssignmentStatement* asgn = new AST::AssignmentStatement{};
	asgn->identifier.name = variable;
	asgn->asgnvalue = write->expr;
	asgn->constasgn = false;

	std::vector<AST::Statement*> follow;

	if (channel_size > 1) {
		AST::Index* index = new AST::Index{};
		AST::Expression* e = new AST::Expression{};
		index->index = e;
		asgn->indices.push_back(index);
		AST::Identifier* ident = new AST::Identifier{};
		ident->identifier = variable_index;
		AST::Identifier* ident2 = new AST::Identifier{};
		ident2->identifier = variable_sz;
		AST::Operator* ops = new AST::Operator{};
		ops->ops = "%";
		AST::Expression* br = new AST::Expression{};
		br->brakets = true;
		AST::Operator* ops2 = new AST::Operator{};
		ops2->ops = "+";
		ops2->left = ident;
		ops2->right = ident2;
		br->child = ops2;
		ops->left = br;
		AST::Literal* l = new AST::Literal{};
		l->literal = std::to_string(channel_size);
		ops->right = l;
		e->child = ops;
	}

	/* Update size */

	AST::Identifier* x = new AST::Identifier{};
	AST::Literal* l1 = new AST::Literal{};
	l1->literal = std::to_string(1);
	x->identifier = variable_sz;
	AST::Operator* ops3 = new AST::Operator{};
	ops3->ops = "+";
	ops3->left = x;
	ops3->right = l1;
	AST::AssignmentStatement* o = new AST::AssignmentStatement{};
	o->identifier.name = variable_sz;
	o->constasgn = false;
	AST::Expression* y = new AST::Expression{};
	y->child = ops3;
	o->asgnvalue = y;
	follow.push_back(o);

	replace_statement(stmt_list, write, asgn, follow);

	delete write;
}

void AST_Transform::replace_identifiers_expr(
	AST::BaseExpression* expr,
	std::map<std::string, std::string>& replacements)
{
	assert(expr != nullptr);
	if (dynamic_cast<AST::Expression*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Expression*>(expr);
		replace_identifiers_expr(tmp->child, replacements);
	}
	else if (dynamic_cast<AST::Identifier*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Identifier*>(expr);
		if (replacements.contains(tmp->identifier)) {
			tmp->identifier = replacements[tmp->identifier];
		}
		for (auto index : tmp->indices) {
			replace_identifiers_expr(index->index, replacements);
		}
		if (tmp->call != nullptr) {
			for (auto p : tmp->call->parameters) {
				replace_identifiers_expr(p, replacements);
			}
		}
	}
	else if (dynamic_cast<AST::Operator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::Operator*>(expr);
		replace_identifiers_expr(tmp->left, replacements);
		replace_identifiers_expr(tmp->right, replacements);
	}
	else if (dynamic_cast<AST::TernaryOperator*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::TernaryOperator*>(expr);
		replace_identifiers_expr(tmp->cond, replacements);
		replace_identifiers_expr(tmp->ifblock, replacements);
		replace_identifiers_expr(tmp->elseblock, replacements);
	}
	else if (dynamic_cast<AST::Literal*>(expr) != nullptr) {
		/* empty */
	}
	else if (dynamic_cast<AST::ListComprehension*>(expr) != nullptr) {
		auto tmp = dynamic_cast<AST::ListComprehension*>(expr);
		for (auto g : tmp->generators) {
			replace_identifiers_expr(g->start, replacements);
			replace_identifiers_expr(g->end, replacements);
		}
		for (auto e : tmp->expressions) {
			replace_identifiers_expr(e, replacements);
		}
	}
	else {
		assert((dynamic_cast<AST::PortPreview*>(expr) != nullptr) || (dynamic_cast<AST::PortSize*>(expr) != nullptr));
	}
}

void AST_Transform::replace_identifiers_vardef(
	std::vector<AST::VarDefinition*>& vardefs,
	std::map<std::string, std::string>& replacements)
{
	for (auto v : vardefs) {
		if (replacements.contains(v->name.name)) {
			v->name.name = replacements[v->name.name];
		}
		if (v->assign != nullptr) {
			replace_identifiers_expr(v->assign, replacements);
		}
		for (auto array : v->arrays) {
			replace_identifiers_expr(array, replacements);
		}
	}
}

void AST_Transform::replace_identifiers_stmt(
	std::vector<AST::Statement*>& statements,
	std::map<std::string, std::string>& replacements)
{
	for (auto s : statements) {
		if (dynamic_cast<AST::AssignmentStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::AssignmentStatement*>(s);
			if (replacements.contains(tmp->identifier.name)) {
				tmp->identifier.name = replacements[tmp->identifier.name];
			}
			if (tmp->asgnvalue != nullptr) {
				replace_identifiers_expr(tmp->asgnvalue, replacements);
			}
			for (auto index : tmp->indices) {
				replace_identifiers_expr(index->index, replacements);
			}
		}
		else if (dynamic_cast<AST::BlockStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::BlockStatement*>(s);
			replace_identifiers_stmt(tmp->statements, replacements);
			replace_identifiers_vardef(tmp->vars, replacements);
		}
		else if (dynamic_cast<AST::CallStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::CallStatement*>(s);
			if (replacements.contains(tmp->name.name)) {
				tmp->name.name = replacements[tmp->name.name];
			}
			for (auto p : tmp->parameters) {
				replace_identifiers_expr(p, replacements);
			}
		}
		else if (dynamic_cast<AST::IfStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::IfStatement*>(s);
			replace_identifiers_expr(tmp->condition, replacements);
			replace_identifiers_stmt(tmp->ifblock, replacements);
			if (tmp->elseblock != nullptr) {
				replace_identifiers_stmt(tmp->elseblock->statements, replacements);
			}
		}
		else if (dynamic_cast<AST::WhileStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::WhileStatement*>(s);
			replace_identifiers_expr(tmp->condition, replacements);
			replace_identifiers_stmt(tmp->statements, replacements);
			replace_identifiers_vardef(tmp->vars, replacements);
		}
		else if (dynamic_cast<AST::ForeachStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::ForeachStatement*>(s);
			for (auto g : tmp->generators) {
				replace_identifiers_expr(g->end, replacements);
				replace_identifiers_expr(g->start, replacements);
			}
			replace_identifiers_stmt(tmp->statements, replacements);
			replace_identifiers_vardef(tmp->vars, replacements);
		}
		else if (dynamic_cast<AST::OutputChannelWriteStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::OutputChannelWriteStatement*>(s);
			replace_identifiers_expr(tmp->expr, replacements);
		}
		else if (dynamic_cast<AST::InputChannelReadStatement*>(s) != nullptr) {
			auto tmp = dynamic_cast<AST::InputChannelReadStatement*>(s);
			/* nothing to do currently */
		}
		else {
			assert(0);
		}
	}
}