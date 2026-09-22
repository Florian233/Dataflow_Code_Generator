#include "Merge_Optimization.hpp"
#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cassert>

#include "IR/AST/AST.hpp"
#include "AST_Transformations.hpp"
#include "Config/config.h"
#include "IR/Composit_Actor.hpp"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"

#include "IR/AST/AST_Printer.hpp"
#include "IR/AST/AST_Helper.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "rapidxml-1.13/rapidxml.hpp"
using namespace rapidxml;

#define USE_MAXLOOP

//#define USE_MUSTPRODUCE //this is not needed, might only improve corner cases, but maybe sometimes nice to have

struct Channel_Variable {
	AST::VarDefinition* channel = nullptr;
	unsigned size = 0;
	AST::VarDefinition* index_r = nullptr;
	AST::VarDefinition* sz = nullptr;
	bool full_clear = false;
	AST::Type* type = nullptr;
};

struct Channel_Dependency {
	IR::Edge* edge;
	double value;
};

using Connector = std::pair<std::string, std::string>;

struct sched_cond_merge {
	std::vector<IR::Actor_Instance*> fusioned;
	std::vector<AST::Statement*> statements_do;
	std::vector<AST::Expression*> overall_guards_do;
	std::vector<AST::Expression*> overall_free_do;
#ifdef USE_MUSTPRODUCE
	std::map<IR::Edge*, AST::Statement*> must_produce_check;
#endif
	std::vector<AST::Statement*> statements_done;
	std::vector<AST::Statement*> statements_init;
	std::vector<unsigned> parent_sched_cond_fusion;
	unsigned id;
	bool added = false;

	std::map<IR::Actor_Instance*, sched_cond_merge*> sub_merges;
};

struct Merge_Data {
	std::map<IR::Actor_Instance*, sched_cond_merge*> sched_cond_fusion;
	/* some entries might be invalid, because they are part of other merges and then deleted!
	 * use with care!
	 */
	std::map<unsigned, sched_cond_merge*> sched_cond_fusion_id;
	unsigned sched_conf_fusion_id_counter = 0;

	std::map<IR::Actor_Instance*, std::vector<IR::Actor_Instance*>> combine_actors;
	std::map<IR::Actor_Instance*, sched_cond_merge*> scm;

	std::map<IR::Edge*, Channel_Variable> channels;
#ifdef USE_MUSTPRODUCE
	std::map<IR::Edge*, unsigned> must_produce;
#endif
	bool all_static = false;
	std::map<IR::Actor*, AST::FSM_Enumeration*> enum_map;
	std::map<IR::Actor_Instance*, std::string> fsmvar;
	std::map<Connector, std::string> new_ports;

	std::map<IR::Edge*, std::vector<Channel_Dependency>> edge_relations;
	std::map<IR::Edge*, unsigned> guaranteed_production;

	std::map<IR::Actor_Instance*, std::map<std::string, std::map<std::string, AST::Expression*>>> inst_action_port_szcheck_map;
	std::map<IR::Actor_Instance*, std::map<std::string, std::map<std::string, AST::Expression*>>> inst_action_port_freecheck_map;

	std::map<IR::Edge*, bool> freecheck_required;

	std::set<std::string> used_symbols;
	std::set<std::string> used_symbols_init;
	std::set<std::string> used_symbols_do;
	std::set<std::string> used_symbols_done;
	std::set<std::string> used_symbols_initialize;
	std::map<IR::Actor_Instance*, std::map<std::string, std::string>> replacements;

	std::vector<IR::Unit*> added_imports;

	std::map<IR::Actor*, bool> has_init;
	std::map<IR::Actor*, bool> has_do;
	std::map<IR::Actor*, bool> has_done;

	std::map<IR::Actor_Instance*, unsigned> loopcount;
	std::map<IR::Actor_Instance*, unsigned> max_loopcount;

	std::map<IR::Edge*, unsigned> minprod;
	std::map<IR::Edge*, unsigned> maxprod;
	std::map<IR::Edge*, unsigned> mincons;
	std::map<IR::Edge*, unsigned> maxcons;
};

static bool is_external_input(
	IR::Edge* edge,
	IR::Actor_Instance* inst)
{
	auto src = dynamic_cast<IR::Actor_Instance*>(edge->get_source());
	if ((src != nullptr) && (src->get_composit_actor() == inst->get_composit_actor())) {
		return false;
	}
	return true;
}

/* Go through predecessors, and if part of the merge, find a synchronous FSM, if found return, otherwise nullptr */
static AST::FSM_Enumeration* get_fsmenum(
	IR::Actor_Instance* a,
	Merge_Data* data,
	IR::Composit_Actor *composit)
{
	if (data->enum_map.contains(a->get_actor())) {
		return data->enum_map[a->get_actor()];
	} else {
		AST::FSM_Enumeration* e = new AST::FSM_Enumeration{};
		std::set<std::string> r;
		a->get_actor()->get_all_states(r);
		for (auto s : r) {
			e->states.push_back(s);
		}
		std::string name = a->get_actor()->get_class_name();
		if (name.find_last_of('.') != std::string::npos) {
			name = name.substr(name.find_last_of('.') + 1);
		}
		name[0] = std::toupper(name[0]);
		e->name = name + "_FSM";
		e->inital_state = a->get_actor()->get_initial_state();
		composit->get_ast()->actor->fsm_enums.push_back(e);
		data->enum_map[a->get_actor()] = e;
		return e;
	}
}

AST::AssignmentStatement* generate_fsm_update(
	std::string statevardef,
	std::string nextstate,
	std::string fsmname)
{
	AST::AssignmentStatement* a = new AST::AssignmentStatement{};
	a->identifier.name = statevardef;
	AST::Expression* asn = new AST::Expression{};
	AST::FSM_Enumeration_Element* e = new AST::FSM_Enumeration_Element{};
	e->enum_element = nextstate;
	e->enum_name = fsmname;
	asn->child = e;
	a->asgnvalue = asn;
	return a;
}

/* Generate channel in variable size computation expression */
static AST::Expression* generate_channel_var_sz(
	Channel_Variable* cv)
{
	AST::Expression* c = new AST::Expression{};
	AST::Identifier* indent = new AST::Identifier{};
	indent->identifier = cv->sz->name.name;
	c->child = indent;
	return c;
}

/* Generate channel in variable free size computation expression */
static AST::Expression* generate_channel_var_free(
	Channel_Variable* cv)
{
	AST::Expression* c = new AST::Expression{};
	c->brakets = true;
	AST::Operator* ops = new AST::Operator{};
	ops->ops = "-";
	AST::Literal* l = new AST::Literal{};
	l->literal = std::to_string(cv->size);
	ops->left = l;
	AST::Identifier* ident = new AST::Identifier{};
	ident->identifier = cv->sz->name.name;
	ops->right = ident;
	c->child = ops;
	return c;
}

bool is_related(
	Merge_Data* data,
	IR::Edge* edge1,
	IR::Edge* edge2)
{
	auto x = data->edge_relations[edge1];
	for (auto c : x) {
		if (c.edge == edge2) {
			return true;
		}
	}
	return false;
}

static bool is_overflow_compensated(
	IR::Actor* actor,
	std::set<std::string> ports,
	std::string state,
	std::set<std::string>& uncompensated)
{
	std::set<std::string> ports_tmp = ports;

	for (auto action : actor->find_schedulable_actions(state)) {
		unsigned compensate = 0;
		for (auto port : action->get_in_buffers()) {
			if (port.tokenrate != 0) {
				if (!ports.contains(port.buffer_name)) {
					compensate++;
					break;
				}
			}
		}

		if (compensate == 1) {
			for (auto port : action->get_in_buffers()) {
				if ((port.tokenrate != 0) && ports_tmp.contains(port.buffer_name)) {
					ports_tmp.erase(port.buffer_name);
				}
			}
		}
	}

	for (auto it : ports_tmp) {
		uncompensated.insert(it);
	}

	return ports_tmp.empty();
}

static bool is_underflow_compensated(
	IR::Actor* actor,
	std::set<std::string> underflow_ports,
	std::string state,
	std::set<std::string> remaining_action_ports,
	std::set<std::string>& uncompensated)
{
	std::set<std::string> ports_tmp = remaining_action_ports;

	for (auto action : actor->find_schedulable_actions(state)) {
		bool compensate = true;
		for (auto port : action->get_in_buffers()) {
			if (port.tokenrate != 0) {
				if (!underflow_ports.contains(port.buffer_name)) {
					compensate = false;
					break;
				}
			}
		}

		if (compensate == true) {
			for (auto port : action->get_in_buffers()) {
				if ((port.tokenrate != 0) && ports_tmp.contains(port.buffer_name)) {
					ports_tmp.erase(port.buffer_name);
				}
			}
		}
	}

	for (auto it : ports_tmp) {
		uncompensated.insert(it);
	}

	return ports_tmp.empty();
}

static bool is_relation_compensated(
	std::map<std::string, IR::Edge*>& edge_map,
	IR::Action* action,
	Merge_Data* data,
	std::set<std::string> ports_to_compensate,
	std::set<std::string>& uncompensated)
{
	bool ret = true;
	for (auto port : ports_to_compensate) {
		bool not_comp = true;
		for (auto in : action->get_in_buffers()) {
			if (in.tokenrate == 0) {
				continue;
			}
			if (!ports_to_compensate.contains(in.buffer_name)) {
				if (is_related(data, edge_map[port], edge_map[in.buffer_name])) {
					not_comp = false;
				}
			}
		}
		if (not_comp) {
			ret = false;
			uncompensated.insert(port);
		}
	}
	return ret;
}

static std::pair<unsigned, unsigned> get_min_max_production(
	IR::Actor* actor,
	std::string port)
{
	unsigned min = 999999;
	unsigned max = 0;

	for (auto action : actor->get_actions()) {
		for (auto out : action->get_out_buffers()) {
			if (out.buffer_name == port) {
				if (out.tokenrate > max) {
					max = out.tokenrate;
				}
				if (out.tokenrate < min) {
					min = out.tokenrate;
				}
			}
		}
	}

	return std::make_pair(min, max);
}

static void get_min_max_consumption(
	IR::Actor* actor,
	std::map<std::string,unsigned>& mincons,
	std::map<std::string, unsigned>& maxcons)
{
	for (auto action : actor->get_actions()) {
		for (auto in : action->get_in_buffers()) {
			if (mincons.contains(in.buffer_name)) {
				if (mincons[in.buffer_name] > in.tokenrate) {
					mincons[in.buffer_name] = in.tokenrate;
				}
			}
			else {
				mincons[in.buffer_name] = in.tokenrate;
			}

			if (maxcons.contains(in.buffer_name)) {
				if (maxcons[in.buffer_name] < in.tokenrate) {
					maxcons[in.buffer_name] = in.tokenrate;
				}
			}
			else {
				maxcons[in.buffer_name] = in.tokenrate;
			}
		}
	}
}

static void register_channel(
	Merge_Data* data,
	AST::Type* type,
	IR::Edge *edge,
	unsigned size,
	bool fullclear)
{
	Channel_Variable cv;
	cv.size = size;
	cv.full_clear = fullclear;
	cv.type = type;
	data->channels[edge] = cv;
}

static void create_channel(
	Channel_Variable& cv,
	IR::Edge* edge)
{
	cv.channel = new AST::VarDefinition{};
	cv.channel->name.name = edge->get_name();
	AST::Type type_copy{ *cv.type };
	cv.channel->type = type_copy;
	cv.channel->constassign = false;
	if (cv.size > 1) {
		AST::Expression* sz_sz = new AST::Expression{};
		AST::Literal* sz_l = new AST::Literal{};
		sz_l->literal = std::to_string(cv.size);
		sz_sz->child = sz_l;
		cv.channel->arrays.push_back(sz_sz);

		cv.index_r = new AST::VarDefinition{};
		cv.index_r->constassign = false;
		cv.index_r->name.name = edge->get_name() + "_index";
		cv.index_r->type.type.name = "uint";
		AST::Expression* e = new AST::Expression{};
		AST::Literal* ll = new AST::Literal{};
		ll->literal = "0";
		e->child = ll;
		cv.index_r->assign = e;
	}
	cv.sz = new AST::VarDefinition{};
	cv.sz->constassign = false;
	cv.sz->type.type.name = "uint";
	cv.sz->name.name = edge->get_name() + "_sz";
	AST::Expression* sz_init = new AST::Expression{};
	AST::Literal* l = new AST::Literal{};
	l->literal = "0";
	sz_init->child = l;
	cv.sz->assign = sz_init;
}

static void determine_channel_sizes(
	Merge_Data* data,
	std::vector<IR::Actor_Instance*>& instances)
{
	Config* c = Config::getInstance();

	for (auto inst : instances) {
		unsigned loopcount = 1;
		std::map<std::string, unsigned> mincons;
		std::map<std::string, unsigned> maxcons;
		get_min_max_consumption(inst->get_actor(), mincons, maxcons);
		std::map<std::string, unsigned> minprod;
		std::map<std::string, unsigned> maxprod;

		/* Ignore channels that are input! */
		std::set<IR::Edge*> inputs;
		std::map<std::string, IR::Edge*> port_edge_map;
		std::map<IR::Edge*, unsigned> edge_size_map;
		for (auto inedge : inst->get_in_edges()) {
			port_edge_map[inedge->get_dst_port()] = inedge;
			auto b = inedge->get_source();
			if (dynamic_cast<IR::Actor_Instance*>(b) != nullptr) {
				auto i = dynamic_cast<IR::Actor_Instance*>(b);
				if (i->get_composit_actor() != inst->get_composit_actor()) {
					inputs.insert(inedge);
				}
				else {
					auto p = get_min_max_production(i->get_actor(), inedge->get_src_port());
					minprod[inedge->get_dst_port()] = p.first;
					maxprod[inedge->get_dst_port()] = p.second;

					data->minprod[inedge] = p.first;
					data->maxprod[inedge] = p.second;
				}
			}
			else {
				inputs.insert(inedge);
			}
		}

		for (auto x : mincons) {
			data->mincons[port_edge_map[x.first]] = x.second;
		}
		for (auto x : maxcons) {
			data->maxcons[port_edge_map[x.first]] = x.second;
		}

		/* Determine SI */
		bool single_in = (inst->get_in_edges().size() == 1);

		if (!single_in) {
			std::map<std::string, unsigned> channelsz;
			std::set<std::string> states;
			inst->get_actor()->get_all_states(states);
			for (auto state : states) {
				/* Determine channels that have nullconsumption */
				std::set<std::string> nocompensation;
				for (auto action : inst->get_actor()->find_schedulable_actions(state)) {
					std::set<std::string> channels_to_compensate;
					for (auto in : action->get_in_buffers()) {
						auto edge = port_edge_map[in.buffer_name];
						if (inputs.contains(edge)) {
							continue;
						}
						if (in.tokenrate == 0) {
							channels_to_compensate.insert(in.buffer_name);
						}
					}
					std::set<std::string> tmp;
					bool v = is_relation_compensated(port_edge_map, action, data, channels_to_compensate, tmp);
					if (!v) {
						is_overflow_compensated(inst->get_actor(), tmp, state, nocompensation);
					}
				}

				/* Determine channels that have nullproduction */
				std::set<std::string> nullproduction;
				for (auto inedge : inst->get_in_edges()) {
					if (minprod.contains(inedge->get_dst_port()) && (minprod[inedge->get_dst_port()] == 0)) {
						nullproduction.insert(inedge->get_dst_port());
					}
				}

				for (auto action : inst->get_actor()->find_schedulable_actions(state)) {
					std::set<std::string> problem_channels;
					std::set<std::string> other_channels;
					for (auto p : action->get_in_buffers()) {
						if (p.tokenrate == 0) {
							continue;
						}
						if (nullproduction.contains(p.buffer_name)) {
							problem_channels.insert(p.buffer_name);
						}
						else {
							other_channels.insert(p.buffer_name);
						}
					}
					std::set<std::string> tmp;
					bool v = is_relation_compensated(port_edge_map, action, data, problem_channels, tmp);
					if (!v) {
						is_overflow_compensated(inst->get_actor(), tmp, state, nocompensation);
					}
				}

				for (auto issue : nocompensation) {
					auto edge = port_edge_map[issue];
					if (inputs.contains(edge)) {
						continue;
					}
					if (edge->get_specified_size() != 0) {
						channelsz[issue] = edge->get_specified_size();
					}
					else {
						channelsz[issue] = c->get_FIFO_size();
					}
				}
				for (auto in : inst->get_in_edges()) {
					if (inputs.contains(in)) {
						continue;
					}
					if (nocompensation.contains(in->get_dst_port())) {
						continue;
					}
					const std::string& port = in->get_dst_port();
					unsigned s;
					if (!in->get_feedback() &&
						(minprod[port] == maxprod[port]) &&
						(mincons[port] == maxcons[port]) &&
						(minprod[port] == mincons[port]) &&
						(minprod[port] != 0)) {
						s = std::max(maxprod[port], maxcons[port]);
					}
					else {
						s = std::max(2 * maxprod[port], 2 * maxcons[port]);
					}
					if (channelsz.contains(in->get_dst_port())) {
						if (channelsz[in->get_dst_port()] < s) {
							channelsz[in->get_dst_port()] = s;
						}
					}
					else {
						channelsz[in->get_dst_port()] = s;
					}
				}
			}

			for (auto it : channelsz) {
				AST::Type* type = nullptr;
				for (auto port : inst->get_ast()->actor->inports) {
					if (port->name.name == it.first) {
						type = &port->type;
					}
				}
				register_channel(data, type, port_edge_map[it.first], it.second, false);
			}
		}
		else {
			IR::Edge* edge = inst->get_in_edges().front();
			if (!inputs.contains(edge)) {
				unsigned size;
				bool full_clear;
				const std::string& portname = edge->get_dst_port();
				/* Resolve the type from the actually connected port */
				AST::Type* type = nullptr;
				for (auto port : inst->get_ast()->actor->inports) {
					if (port->name.name == portname) {
						type = &port->type;
					}
				}
				if ((minprod[portname] == maxprod[portname]) &&
					(maxcons[portname] == mincons[portname]))
				{
					if (minprod[portname] == mincons[portname]) {
						full_clear = true;
						size = minprod[portname];
					}
					else {
						full_clear = maxprod[portname] % maxcons[portname] == 0;
						size = std::max(2 * maxprod[portname], 2 * maxcons[portname]);
					}
				}
				else {
					full_clear = false;
					size = std::max(2 * maxprod[portname], 2 * maxcons[portname]);
				}
				register_channel(data, type, edge, size, full_clear);
			}
		}
		data->loopcount[inst] = loopcount; /* always one currently, only for init purposes, probably a leftover */
		data->max_loopcount[inst] = loopcount; /* always one currently, only for init purposes, probably a leftover */
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Channel sizes:";
	for (auto x : data->channels) {
		std::cout << " (" << x.first->get_name() << ", " << x.second.size << ")";
	}
	std::cout << std::endl;
#endif
}

static void determine_required_freechecks(
	std::vector<IR::Actor_Instance*>& cluster,
	Merge_Data* data)
{
	for (auto inst : cluster) {
		for (auto out : inst->get_out_edges()) {
			IR::Actor_Instance* dst = dynamic_cast<IR::Actor_Instance*>(out->get_sink());
			if ((dst == nullptr) || (inst->get_composit_actor() != dst->get_composit_actor())) {
				data->freecheck_required[out] = true;
				continue;
			}
			if ((data->mincons[out] == 0) && (data->max_loopcount[dst] == 1)) {
				data->freecheck_required[out] = true;
			}
			else {
				data->freecheck_required[out] = false;
			}
		}
	}
}

static void fix_scope(
	AST::Statement* s,
	std::vector<AST::Statement*>& next,
	unsigned depth)
{
	if (depth == 0) {
		next.push_back(s);
		return;
	}
	--depth;

	if (dynamic_cast<AST::BlockStatement*>(s) != nullptr) {
		auto b = dynamic_cast<AST::BlockStatement*>(s);
		if (b->vars.empty()) {
			for (auto x : b->statements) {
				fix_scope(x, next, depth);
			}
		}
		else {
			std::vector<AST::Statement*> tmp;
			next.push_back(s);
			for (auto x : b->statements) {
				fix_scope(x, tmp, depth);
			}
			b->statements.swap(tmp);
		}
	}
	else if (dynamic_cast<AST::IfStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::IfStatement*>(s);
		if (dynamic_cast<AST::Literal*>(i->condition->child) != nullptr) {
			auto l = dynamic_cast<AST::Literal*>(i->condition->child);
			if (l->literal == "true") {
				for (auto x : i->ifblock) {
					fix_scope(x, next, depth);
				}
			}
			else {
				next.push_back(s);
				std::vector<AST::Statement*> tmp;
				for (auto x : i->ifblock) {
					fix_scope(x, tmp, depth);
				}
				i->ifblock.swap(tmp);
				if (i->elseblock != nullptr) {
					tmp.clear();
					for (auto x : i->elseblock->statements) {
						fix_scope(x, tmp, depth);
					}
					i->elseblock->statements.swap(tmp);
				}
			}
		}
		else {
			next.push_back(s);
			std::vector<AST::Statement*> tmp;
			for (auto x : i->ifblock) {
				fix_scope(x, tmp, depth);
			}
			i->ifblock.swap(tmp);
			if (i->elseblock != nullptr) {
				tmp.clear();
				for (auto x : i->elseblock->statements) {
					fix_scope(x, tmp, depth);
				}
				i->elseblock->statements.swap(tmp);
			}
		}
	}
	else if (dynamic_cast<AST::ForeachStatement*>(s) != nullptr) {
		auto f = dynamic_cast<AST::ForeachStatement*>(s);
		std::vector<AST::Statement*> tmp;
		next.push_back(s);
		for (auto x : f->statements) {
			fix_scope(x, tmp, depth);
		}
		f->statements.swap(tmp);
	}
	else {
		next.push_back(s);
	}
}

static void remove_useless_scopes(
	std::vector<AST::Statement*>& stmts,
	unsigned dept = 9999)
{
	std::vector<AST::Statement*> tmp;
	for (auto s : stmts) {
		fix_scope(s, tmp, dept);
	}
	stmts.swap(tmp);
}

static bool determine_cycle(
	std::map<std::string, std::set<std::string>>& next_states,
	std::string search,
	std::string child,
	std::string initial_state,
	std::set<std::string> processed)
{
	if (child == initial_state) {
		return false;
	}
	if (child == search) {
		return true;
	}
	if (processed.contains(child)) {
		return false;
	}
	processed.insert(child);

	for (auto n : next_states[child]) {
		if (determine_cycle(next_states, search, n, initial_state, processed)) {
			return true;
		}
	}
	return false;
}

static void get_phase_actions(
	IR::Actor* actor,
	std::map<std::string, std::vector<IR::Action*>>& actions_init_out,
	std::map<std::string, std::vector<IR::Action*>>& actions_do_out,
	std::map<std::string, std::vector<IR::Action*>>& actions_done_out)
{
	if (actor->get_fsm().empty()) {
		std::vector<IR::Action*> dolist;
		for (auto action : actor->get_actions()) {
			if (action->is_init()) {
				continue;
			}
			dolist.push_back(action);
		}
		actions_do_out[""] = dolist;
		return;
	}

	Config* c = Config::getInstance();
	if (!c->get_prolog_epilog_opt()) {
		std::set<std::string> states;
		actor->get_all_states(states);

		for (auto state : states) {
			auto x = actor->find_schedulable_actions(state);
			actions_do_out[state] = x;
		}

		return;
	}

	std::map<std::string, std::set<std::string>> next_states;

	for (auto entry : actor->get_fsm()) {
		if (!next_states.contains(entry.state)) {
			std::set<std::string> n;
			n.insert(entry.next_state);
			next_states[entry.state] = n;
		}
		else {
			next_states[entry.state].insert(entry.next_state);
		}
	}

	std::set<std::string> do_cycle;
	std::string init_state = actor->get_initial_state();
	std::set<std::string> states_to_process;
	actor->get_all_states(states_to_process);

	for (auto state : states_to_process) {
		if (state == init_state) {
			continue;
		}
		for (auto n : next_states[state]) {
			if (determine_cycle(next_states, state, n, init_state, std::set<std::string>())) {
				do_cycle.insert(state);
				break;
			}
		}
	}

	/* didn't find a cycle the initial state is not involved in, hence,
	 * all actions go to the do_cycle, nothing to exclude
	 */
	if (do_cycle.empty()) {
		for (auto x : actor->get_fsm()) {
			if (actions_do_out.contains(x.state)) {
				actions_do_out[x.state].push_back(actor->get_action(x.action));
			}
			else {
				std::vector<IR::Action*> dolist;
				dolist.push_back(actor->get_action(x.action));
				actions_do_out[x.state] = dolist;
			}
		}
		return;
	}

	std::set<std::string> done_cycle;
	for (auto x : do_cycle) {
		auto next = next_states[x];
		for (auto n : next) {
			if (!do_cycle.contains(n) && (n != init_state)) {
				done_cycle.insert(n);
			}
		}
	}

	/* done might have further states not directly connected to do, hence, start another search until we find only init or do */
	std::set<std::string> runner = done_cycle;
	while (!runner.empty()) {
		auto it = runner.begin();
		std::string state = *it;
		runner.erase(it);
		auto next = next_states[state];
		for (auto n : next) {
			if (!do_cycle.contains(n) && (n != init_state) && !done_cycle.contains(n)) {
				done_cycle.insert(n);
				runner.insert(n);
			}
		}
	}

	/* the remaining states must be init states */
	for (auto x : actor->get_fsm()) {
		if (do_cycle.contains(x.state)) {
			if (actions_do_out.contains(x.state)) {
				actions_do_out[x.state].push_back(actor->get_action(x.action));
			}
			else {
				std::vector<IR::Action*> list;
				list.push_back(actor->get_action(x.action));
				actions_do_out[x.state] = list;
			}
		}
		else if (done_cycle.contains(x.state)) {
			if (actions_done_out.contains(x.state)) {
				actions_done_out[x.state].push_back(actor->get_action(x.action));
			}
			else {
				std::vector<IR::Action*> list;
				list.push_back(actor->get_action(x.action));
				actions_done_out[x.state] = list;
			}
		}
		else {
			if (actions_init_out.contains(x.state)) {
				actions_init_out[x.state].push_back(actor->get_action(x.action));
			}
			else {
				std::vector<IR::Action*> list;
				list.push_back(actor->get_action(x.action));
				actions_init_out[x.state] = list;
			}
		}
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


static AST::InputChannelReadStatement* find_readstatement(
	AST::Statement* s,
	std::string port,
	std::vector<AST::Statement*>** stmt_list)
{
	if (dynamic_cast<AST::InputChannelReadStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::InputChannelReadStatement*>(s);
		if (i->port.name == port) {
			return i;
		}
	}
	else if (dynamic_cast<AST::BlockStatement*>(s) != nullptr) {
		auto b = dynamic_cast<AST::BlockStatement*>(s);
		for (auto stmt : b->statements) {
			auto r = find_readstatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &b->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::WhileStatement*>(s) != nullptr) {
		auto w = dynamic_cast<AST::WhileStatement*>(s);
		for (auto stmt : w->statements) {
			auto r = find_readstatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &w->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::ForeachStatement*>(s) != nullptr) {
		auto f = dynamic_cast<AST::ForeachStatement*>(s);
		for (auto stmt : f->statements) {
			auto r = find_readstatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &f->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::IfStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::IfStatement*>(s);
		for (auto stmt : i->ifblock) {
			auto r = find_readstatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &i->ifblock;
				}
				return r;
			}
		}
		if (i->elseblock != nullptr) {
			for (auto stmt : i->elseblock->statements) {
				auto r = find_readstatement(stmt, port, stmt_list);
				if (r != nullptr) {
					if (*stmt_list == nullptr) {
						*stmt_list = &i->elseblock->statements;
					}
					return r;
				}
			}
		}
	}
	else {
		/* not relevant here */
	}
	return nullptr;
}

static AST::InputChannelReadStatement* find_port_read(
	AST::Action* action,
	std::string port,
	std::vector<AST::Statement*>** stmt_list)
{
	for (auto it = action->statements.begin(); it != action->statements.end(); ++it) {
		AST::InputChannelReadStatement* s = find_readstatement(*it, port, stmt_list);
		if (s != nullptr) {
			if (*stmt_list == nullptr) {
				*stmt_list = &action->statements;
			}
			return s;
		}
	}
	return nullptr;
}

static std::string find_portname(
	AST::BaseExpression* expr,
	AST::BaseExpression*** b)
{
	if (dynamic_cast<AST::PortFree*>(expr) != nullptr) {
		auto p = dynamic_cast<AST::PortFree*>(expr);
		return p->port;
	}
	else if (dynamic_cast<AST::PortSize*>(expr) != nullptr) {
		auto p = dynamic_cast<AST::PortSize*>(expr);
		return p->port;
	}
	else if (dynamic_cast<AST::Expression*>(expr) != nullptr) {
		*b = &(dynamic_cast<AST::Expression*>(expr)->child);
		return find_portname(dynamic_cast<AST::Expression*>(expr)->child, b);
	}
	else if (dynamic_cast<AST::Operator*>(expr) != nullptr) {
		auto o = dynamic_cast<AST::Operator*>(expr);
		auto r = find_portname(o->left, b);
		if (!r.empty()) {
			*b = &o->left;
			return r;
		}
		else {
			*b = &o->right;
			return find_portname(o->right, b);
		}
	}
	else {
		/* should be irrelevant for this case */
	}
	return "";
}

static void rename_port_in_expr(
	AST::BaseExpression* expr,
	const std::string& oldp,
	const std::string& newp)
{
	if (expr == nullptr) {
		return;
	}
	if (auto p = dynamic_cast<AST::PortSize*>(expr)) {
		if (p->port == oldp) { p->port = newp; }
	}
	else if (auto p = dynamic_cast<AST::PortFree*>(expr)) {
		if (p->port == oldp) { p->port = newp; }
	}
	else if (auto p = dynamic_cast<AST::PortPreview*>(expr)) {
		if (p->port == oldp) { p->port = newp; }
		if (p->index != nullptr) { rename_port_in_expr(p->index->index, oldp, newp); }
	}
	else if (auto e = dynamic_cast<AST::Expression*>(expr)) {
		rename_port_in_expr(e->child, oldp, newp);
	}
	else if (auto o = dynamic_cast<AST::Operator*>(expr)) {
		rename_port_in_expr(o->left, oldp, newp);
		rename_port_in_expr(o->right, oldp, newp);
	}
	else if (auto t = dynamic_cast<AST::TernaryOperator*>(expr)) {
		rename_port_in_expr(t->cond, oldp, newp);
		rename_port_in_expr(t->ifblock, oldp, newp);
		rename_port_in_expr(t->elseblock, oldp, newp);
	}
	else if (auto i = dynamic_cast<AST::Identifier*>(expr)) {
		for (auto index : i->indices) {
			rename_port_in_expr(index->index, oldp, newp);
		}
		if (i->call != nullptr) {
			for (auto pr : i->call->parameters) {
				rename_port_in_expr(pr, oldp, newp);
			}
		}
	}
	else if (auto lc = dynamic_cast<AST::ListComprehension*>(expr)) {
		for (auto g : lc->generators) {
			rename_port_in_expr(g->start, oldp, newp);
			rename_port_in_expr(g->end, oldp, newp);
		}
		for (auto e : lc->expressions) {
			rename_port_in_expr(e, oldp, newp);
		}
	}
	else {
		/* no port reference */
	}
}

static void rename_port_in_stmts(
	std::vector<AST::Statement*>& statements,
	const std::string& oldp,
	const std::string& newp)
{
	for (auto s : statements) {
		if (auto r = dynamic_cast<AST::InputChannelReadStatement*>(s)) {
			if (r->port.name == oldp) { r->port.name = newp; }
			if (r->index != nullptr) { rename_port_in_expr(r->index->index, oldp, newp); }
		}
		else if (auto w = dynamic_cast<AST::OutputChannelWriteStatement*>(s)) {
			if (w->port.name == oldp) { w->port.name = newp; }
			rename_port_in_expr(w->expr, oldp, newp);
		}
		else if (auto b = dynamic_cast<AST::BlockStatement*>(s)) {
			rename_port_in_stmts(b->statements, oldp, newp);
		}
		else if (auto i = dynamic_cast<AST::IfStatement*>(s)) {
			rename_port_in_expr(i->condition, oldp, newp);
			rename_port_in_stmts(i->ifblock, oldp, newp);
			if (i->elseblock != nullptr) {
				rename_port_in_stmts(i->elseblock->statements, oldp, newp);
			}
		}
		else if (auto wst = dynamic_cast<AST::WhileStatement*>(s)) {
			rename_port_in_expr(wst->condition, oldp, newp);
			rename_port_in_stmts(wst->statements, oldp, newp);
		}
		else if (auto f = dynamic_cast<AST::ForeachStatement*>(s)) {
			for (auto g : f->generators) {
				rename_port_in_expr(g->start, oldp, newp);
				rename_port_in_expr(g->end, oldp, newp);
			}
			rename_port_in_stmts(f->statements, oldp, newp);
		}
		else if (auto a = dynamic_cast<AST::AssignmentStatement*>(s)) {
			if (a->asgnvalue != nullptr) { rename_port_in_expr(a->asgnvalue, oldp, newp); }
			for (auto index : a->indices) {
				rename_port_in_expr(index->index, oldp, newp);
			}
		}
		else if (auto c = dynamic_cast<AST::CallStatement*>(s)) {
			for (auto p : c->parameters) {
				rename_port_in_expr(p, oldp, newp);
			}
		}
		else {
			/* no port reference */
		}
	}
}

static void rename_port_in_action(
	AST::Action* a,
	const std::string& oldp,
	const std::string& newp)
{
	rename_port_in_stmts(a->statements, oldp, newp);
	for (auto g : a->guards) {
		rename_port_in_expr(g, oldp, newp);
	}
	for (auto g : a->output_guards) {
		rename_port_in_expr(g, oldp, newp);
	}
	for (auto ip : a->input_patterns) {
		if (ip->port.name == oldp) { ip->port.name = newp; }
	}
	for (auto op : a->output_expressions) {
		if (op->port.name == oldp) { op->port.name = newp; }
	}
}

/* Function is called last by do, so do is kept for the following operations. */
static void replace_size_checks(
	IR::Actor_Instance *inst,
	Merge_Data* data,
	std::set<std::string> inports,
	AST::Action *action,
	bool update_map)
{
	for (auto guard : action->guards) {
		AST::BaseExpression** b;
		auto portname = find_portname(guard, &b);
		if (!portname.empty() && !inports.contains(portname)) {
			IR::Edge* edge = nullptr;
			for (auto in : inst->get_in_edges()) {
				if (in->get_dst_port() == portname) {
					edge = in;
				}
			}
			assert(edge != nullptr);
			auto cv = data->channels[edge];
			auto e = generate_channel_var_sz(&cv);
			delete (*b);
			*b = e;
			if (update_map) {
				data->inst_action_port_szcheck_map[inst][action->name.name][portname] = guard;
			}
		}
	}
}

static void remove_expression_from_cond(
	AST::Expression* expr)
{
	/* the expression should be ANDed, hence, we can simply set this one expression to true */
	AST::Literal* l = new AST::Literal{};
	l->literal = "true";
	delete expr->child;
	expr->child = l;
}

static void move_expression_to_scm_freecheck(
	AST::Expression* expr,
	sched_cond_merge* scm)
{
	if (dynamic_cast<AST::Literal*>(expr->child) == nullptr) {
		AST::Expression* e = expr->clone();
		scm->overall_free_do.push_back(e);
		remove_expression_from_cond(expr);
	}
	else {
		/* in this case the expression has been replaced by "true" and therefor shouldn't be added again.
		 * Expressions for free checks cannot have a literal as child in any other case.
		 */
	}
}

static AST::OutputChannelWriteStatement* find_writestatement(
	AST::Statement* s,
	std::string port,
	std::vector<AST::Statement*>** stmt_list)
{
	if (dynamic_cast<AST::OutputChannelWriteStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::OutputChannelWriteStatement*>(s);
		if (i->port.name == port) {
			return i;
		}
	}
	else if (dynamic_cast<AST::BlockStatement*>(s) != nullptr) {
		auto b = dynamic_cast<AST::BlockStatement*>(s);
		for (auto stmt : b->statements) {
			auto r = find_writestatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &b->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::WhileStatement*>(s) != nullptr) {
		auto w = dynamic_cast<AST::WhileStatement*>(s);
		for (auto stmt : w->statements) {
			auto r = find_writestatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &w->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::ForeachStatement*>(s) != nullptr) {
		auto f = dynamic_cast<AST::ForeachStatement*>(s);
		for (auto stmt : f->statements) {
			auto r = find_writestatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &f->statements;
				}
				return r;
			}
		}
	}
	else if (dynamic_cast<AST::IfStatement*>(s) != nullptr) {
		auto i = dynamic_cast<AST::IfStatement*>(s);
		for (auto stmt : i->ifblock) {
			auto r = find_writestatement(stmt, port, stmt_list);
			if (r != nullptr) {
				if (*stmt_list == nullptr) {
					*stmt_list = &i->ifblock;
				}
				return r;
			}
		}
		if (i->elseblock != nullptr) {
			for (auto stmt : i->elseblock->statements) {
				auto r = find_writestatement(stmt, port, stmt_list);
				if (r != nullptr) {
					if (*stmt_list == nullptr) {
						*stmt_list = &i->elseblock->statements;
					}
					return r;
				}
			}
		}
	}
	else {
		/* not relevant here */
	}
	return nullptr;
}

static AST::OutputChannelWriteStatement* find_port_write(
	AST::Action* action,
	std::string port,
	std::vector<AST::Statement*>** stmt_list)
{
	for (auto s : action->statements) {
		auto r = find_writestatement(s, port, stmt_list);
		if (r != nullptr) {
			if (*stmt_list == nullptr) {
				*stmt_list = &action->statements;
			}
			return r;
		}
	}
	return nullptr;
}

static void replace_out_guards(
	IR::Actor_Instance* inst,
	AST::Action* action,
	Merge_Data* data,
	std::set<std::string> outports,
	std::vector<IR::Edge*>& combine_edge,
	bool update_map)
{
	std::vector<AST::Expression*> delete_list;
	for (auto o : action->output_guards) {
		AST::BaseExpression** b;
		std::string portname = find_portname(o, &b);
		assert(!portname.empty());
		if (outports.contains(portname)) {
			/* nothing to do */
		}
		else {
			bool found = false;
			for (auto e : combine_edge) {
				if (e->get_src_port() == portname) {
					/* get rid of it */
					delete_list.push_back(o);
					found = true;
					break;
				}
			}
			if (!found) {
				IR::Edge* edge = nullptr;
				for (auto out : inst->get_out_edges()) {
					if (out->get_src_port() == portname) {
						edge = out;
						break;
					}
				}
				assert(edge != nullptr);
				if (!data->freecheck_required.contains(edge) || !data->freecheck_required[edge]) {
					delete_list.push_back(o);
					continue;
				}

				/* insert size check */
				delete (*b);
				Channel_Variable cv = data->channels[edge];
				auto e = generate_channel_var_free(&cv);
				e->brakets = true;
				*b = e;
				if (update_map) {
					data->inst_action_port_freecheck_map[inst][action->name.name][portname] = o;
				}
			}
		}
	}

	for (auto d : delete_list) {
		auto it = std::find(action->output_guards.begin(), action->output_guards.end(), d);
		if (it != action->output_guards.end()) {
			action->output_guards.erase(it);
		}
	}
}

#ifdef USE_MUSTPRODUCE
static void generate_must_produce_checks(
	IR::Actor_Instance* inst,
	Merge_Data* data,
	std::vector<AST::Statement*>& out,
	sched_cond_merge* scm)
{
	std::map<std::string, unsigned> mp_in;
	std::map<IR::Edge*, unsigned> mp_out;
	std::map<std::string, unsigned> fulfilled_out;
	for (auto in : inst->get_in_edges()) {
		if (data->must_produce.contains(in)) {
			mp_in[in->get_dst_port()] = data->must_produce[in];
		}
	}
	for (auto out : inst->get_out_edges()) {
		if (data->must_produce.contains(out)) {
			mp_out[out] = data->must_produce[out];
		}
	}

	for (auto action : inst->get_actor()->get_actions()) {
		bool all_covered = true;
		for (auto in : action->get_in_buffers()) {
			if (in.tokenrate == 0) {
				continue;
			}
			if (!mp_in.contains(in.buffer_name)) {
				all_covered = false;
				break;
			}
			else if (mp_in[in.buffer_name] < in.tokenrate) {
				all_covered = false;
			}
		}
		if (!all_covered) {
			continue;
		}
		for (auto out : action->get_out_buffers()) {
			if (out.tokenrate == 0) {
				continue;
			}
			if (fulfilled_out.contains(out.buffer_name)) {
				if (fulfilled_out[out.buffer_name] > out.tokenrate) {
					fulfilled_out[out.buffer_name] = out.tokenrate;
				}
			}
			else {
				fulfilled_out[out.buffer_name] = out.tokenrate;
			}
		}
	}
	for (auto m : mp_out) {
		if (!fulfilled_out.contains(m.first->get_src_port()) || (fulfilled_out[m.first->get_src_port()] < m.second)) {
			AST::IfStatement* ifstmt = new AST::IfStatement{};
			AST::Expression* cond = new AST::Expression{};
			auto cv = data->channels[m.first];
			auto sz = generate_channel_var_sz(&cv);
			sz->brakets = true;
			AST::Operator* ops = new AST::Operator{};
			ops->ops = "<";
			AST::Literal* l = new AST::Literal{};
			l->literal = std::to_string(m.second);
			ops->right = l;
			ops->left = sz;
			cond->child = ops;
			ifstmt->condition = cond;
			AST::ReturnStatement* r = new AST::ReturnStatement{};
			ifstmt->ifblock.push_back(r);
			out.push_back(ifstmt);
			scm->must_produce_check[m.first] = ifstmt;
		}
	}
}

static void generate_must_produce_input_check(
	std::string portname,
	unsigned size,
	std::vector<AST::Statement*>& out)
{
	AST::IfStatement* ifstmt = new AST::IfStatement{};
	AST::Expression* cond = new AST::Expression{};
	AST::PortSize* sz = new AST::PortSize{};
	sz->port = portname;
	AST::Operator* ops = new AST::Operator{};
	ops->ops = "<";
	AST::Literal* l = new AST::Literal{};
	l->literal = std::to_string(size);
	ops->right = l;
	ops->left = sz;
	cond->child = ops;
	ifstmt->condition = cond;
	AST::ReturnStatement* r = new AST::ReturnStatement{};
	ifstmt->ifblock.push_back(r);
	out.push_back(ifstmt);
}
#endif

static void transform_actions_to_functions(
	IR::Actor_Instance *inst,
	std::set<std::string> inports,
	std::set<std::string> outports,
	Merge_Data *data,
	IR::Composit_Actor* composit,
	std::map<std::string, std::string>& function_map_out)
{
	/* turn channel accesses into assignments */
	for (auto a : inst->get_ast()->actor->actions) {

		for (auto inp : inst->get_in_edges()) {
			if (!inports.contains(inp->get_dst_port())) {
				auto e = data->channels[inp];
				while (true) {
					std::vector<AST::Statement*>* stmt_list = nullptr;
					auto s = find_port_read(a, inp->get_dst_port(), &stmt_list);
					if (s != nullptr) {
						std::string index_r_name;
						if (e.index_r != nullptr) {
							index_r_name = e.index_r->name.name;
						}
						AST_Transform::channelreadstmt_assignment(a, s, e.channel->name.name, index_r_name, e.sz->name.name, e.size, stmt_list);
					}
					else {
						break;
					}
				}
			}
		}

		for (auto outp : inst->get_out_edges()) {
			if (!outports.contains(outp->get_src_port())) {
				auto e = data->channels[outp];
				while (true) {
					std::vector<AST::Statement*>* stmt_list = nullptr;
					auto s = find_port_write(a, outp->get_src_port(), &stmt_list);
					if (s != nullptr) {
						std::string index_r_name;
						if (e.index_r != nullptr) {
							index_r_name = e.index_r->name.name;
						}
						AST_Transform::channelwritestmt_assignment(a, s, e.channel->name.name, index_r_name, e.sz->name.name, e.size, stmt_list);
					}
					else {
						break;
					}
				}
			}
		}

		std::string function_name = inst->get_actor()->get_class_name();
		if (function_name.find_last_of('.') != std::string::npos) {
			function_name = function_name.substr(function_name.find_last_of('.') + 1);
		}
		function_name[0] = std::toupper(function_name[0]);
		function_name += "_" + a->name.name;
		function_name = inst->get_name() + "_" + function_name;

		AST::Procedure* p = new AST::Procedure{};
		p->vars.insert(p->vars.begin(), a->vars.begin(), a->vars.end());
		p->statements.insert(p->statements.begin(), a->statements.begin(), a->statements.end());
		p->name.name = function_name;

		composit->get_ast()->actor->procedures.push_back(p);

		function_map_out[a->name.name] = function_name;
	}
}

static void transform_actions(
	IR::Actor_Instance* inst,
	Merge_Data* data,
	std::vector<IR::Action*> actions,
	std::set<std::string> inports,
	std::set<std::string> outports,
	std::string state,
	std::vector<IR::Edge*> combine_edge,
	std::vector<AST::Statement*> additional_code,
	std::vector<AST::Statement*>& out,
	sched_cond_merge* scm,
	IR::Composit_Actor *composit,
	std::map<std::string, std::string>& functions,
	bool do_transform = false)
{
	/* compile list of actions from the copied AST of the instance */
	std::vector<AST::Action*> ast_actions;
	for (auto a : actions) {
		for (auto aa : inst->get_ast()->actor->actions) {
			if (aa->name.name == a->get_ast()->name.name) {
				ast_actions.push_back(aa);
			}
		}
	}

	/* if combine edges are present, sort out corresponding size checks */
	for (auto c : combine_edge) {
		auto tmp = data->inst_action_port_szcheck_map[dynamic_cast<IR::Actor_Instance*>(c->get_sink())];
		for (auto a : tmp) {
			auto sz = a.second.find(c->get_dst_port());
			if ((sz != a.second.end()) && (sz->second != nullptr)) {
				remove_expression_from_cond(sz->second);
			}
		}

		auto x = data->inst_action_port_freecheck_map[dynamic_cast<IR::Actor_Instance*>(c->get_sink())];
		for (auto y : x) {
			for (auto q : y.second)
			move_expression_to_scm_freecheck(q.second, scm);
		}
	}

	/* replace size checks with corresponding code, before guard transformation! Also check must produce. */
	for (auto a : ast_actions) {
		replace_size_checks(inst, data, inports, a, do_transform);
		replace_out_guards(inst, a, data, outports, combine_edge, do_transform);
	}

	if (do_transform && inst->get_actor()->is_static()) {

		auto first = ast_actions.front();
		for (auto i : first->guards) {
			for (auto x : data->inst_action_port_szcheck_map[inst][first->name.name]) {
				if (x.second == i) {
					auto expr = x.second;
					if (data->loopcount[inst] > 1) {
						auto ops = dynamic_cast<AST::Operator*>(expr->child);
						auto loopmult = new AST::Operator{};
						loopmult->ops = "*";
						AST::Literal* l = new AST::Literal{};
						l->literal = std::to_string(data->loopcount[inst]);
						loopmult->right = l;
						loopmult->left = ops->right;
						AST::Expression* e = new AST::Expression{};
						e->child = loopmult;
						e->brakets = true;
						ops->right = e;
					}
					scm->overall_guards_do.push_back(expr);
				}
			}
		}

		for (auto i : first->guards) {
			AST::BaseExpression** b;
			std::string portname = find_portname(i, &b);
			if (portname.empty() || !inports.contains(portname)) {
				continue;
			}
			auto expr = i;
			if (data->loopcount[inst] > 1) {
				auto ops = dynamic_cast<AST::Operator*>(expr->child);
				auto loopmult = new AST::Operator{};
				loopmult->ops = "*";
				AST::Literal* l = new AST::Literal{};
				l->literal = std::to_string(data->loopcount[inst]);
				loopmult->right = l;
				loopmult->left = ops->right;
				AST::Expression* e = new AST::Expression{};
				e->child = loopmult;
				e->brakets = true;
				ops->right = e;
			}
			scm->overall_guards_do.push_back(expr);
		}

		for (auto o : first->output_guards) {
			auto expr = o;
			if (data->loopcount[inst] > 1) {
				auto ops = dynamic_cast<AST::Operator*>(expr->child);
				auto loopmult = new AST::Operator{};
				loopmult->ops = "*";
				AST::Literal* l = new AST::Literal{};
				l->literal = std::to_string(data->loopcount[inst]);
				loopmult->right = l;
				loopmult->left = ops->right;
				AST::Expression* e = new AST::Expression{};
				e->child = loopmult;
				e->brakets = true;
				ops->right = e;
			}
			scm->overall_free_do.push_back(expr);
		}

		for (auto a : ast_actions) {
			a->output_guards.clear();

			std::vector<AST::Expression*> to_erase;
			for (auto g : a->guards) {
				bool erase = false;
				for (auto x : data->inst_action_port_szcheck_map[inst][a->name.name]) {
					if (x.second == g) {
						erase = true;
						break;
					}
				}
				if (!erase) {
					AST::BaseExpression** b;
					std::string portname = find_portname(g, &b);
					if (!portname.empty() && inports.contains(portname)) {
						erase = true;
					}
				}
				if (erase) {
					to_erase.push_back(g);
				}
			}
			for (auto erase : to_erase) {
				auto it = std::find(a->guards.begin(), a->guards.end(), erase);
				if (it != a->guards.end()) {
					a->guards.erase(it);
				}
			}
		}
	}

	/* if fsm, add state transition */
	std::map<std::string, std::vector<AST::Statement*>> adds;
	if (!inst->get_actor()->get_fsm().empty()) {
		for (auto a : ast_actions) {
			std::string next = inst->get_actor()->find_next_state(state, a->name.name);
			auto s = generate_fsm_update(data->fsmvar[inst], next, data->enum_map[inst->get_actor()]->name);
			std::vector<AST::Statement*> x;
			x.push_back(s);
			adds[a->name.name] = x;
		}
	}

	if (!additional_code.empty()) {
		for (auto a : ast_actions) {
			if (adds.contains(a->name.name)) {
				adds[a->name.name].insert(adds[a->name.name].end(), additional_code.begin(), additional_code.end());
			}
			else {
				std::vector<AST::Statement*> x;
				x.insert(x.begin(), additional_code.begin(), additional_code.end());
				adds[a->name.name] = x;
			}
		}
	}

	/* transform guards */
	bool add_break = (do_transform) && (data->max_loopcount[inst] != 1) && (ast_actions.size() > 1);
	auto res = AST_Transform::guardprio_cond(inst->get_ast(), ast_actions, adds, functions, add_break);
	out.push_back(res);
}

static void combine_actions(
	IR::Composit_Actor* composit,
	IR::Actor_Instance* inst,
	AST::VarDefinition* fsmdef,
	Merge_Data* data,
	std::map<std::string, std::string>& replacements,
	std::vector<IR::Actor_Instance*>& combine_to)
{
	std::map<std::string, std::vector<IR::Action*>> init_actions;
	std::map<std::string, std::vector<IR::Action*>> do_actions;
	std::map<std::string, std::vector<IR::Action*>> done_actions;
	sched_cond_merge* scm = data->scm[inst];
#ifdef USE_MAXLOOP
	unsigned loopsz = data->max_loopcount[inst]; /* only applicable in do */
#else
	unsigned loopsz = data->loopcount[inst]; /* only applicable in do */
#endif
	bool has_fsm = (fsmdef != nullptr);
	AST::AST_Root* ast = composit->get_ast();

	//AST::print_ast(inst->get_ast());

	get_phase_actions(inst->get_actor(), init_actions, do_actions, done_actions);

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Init actions:\n";
	for (auto x : init_actions) {
		std::cout << "  State: " << x.first << ":";
		for (auto a : x.second) {
			std::cout << " " << a->get_name();
		}
		std::cout << std::endl;
	}

	std::cout << "Do actions:\n";
	for (auto x : do_actions) {
		std::cout << "  State: " << x.first << ":";
		for (auto a : x.second) {
			std::cout << " " << a->get_name();
		}
		std::cout << std::endl;
	}

	std::cout << "Done actions:\n";
	for (auto x : done_actions) {
		std::cout << "  State: " << x.first << ":";
		for (auto a : x.second) {
			std::cout << " " << a->get_name();
		}
		std::cout << std::endl;
	}
#endif

	std::vector<IR::Edge*> combine_edge;
	for (auto c : combine_to) {
		for (auto out : inst->get_out_edges()) {
			if (dynamic_cast<IR::Actor_Instance*>(out->get_sink()) == c) {
				combine_edge.push_back(out);
			}
		}
	}

	std::vector<AST::Statement*> additional_code;
	for (auto z : scm->sub_merges) {
		additional_code.insert(additional_code.end(), z.second->statements_do.begin(), z.second->statements_do.end());

		if (!z.second->statements_done.empty()) {
			scm->statements_done.insert(scm->statements_done.end(), z.second->statements_done.begin(), z.second->statements_done.end());
		}
		if (!z.second->statements_init.empty()) {
			scm->statements_init.insert(scm->statements_init.end(), z.second->statements_init.begin(), z.second->statements_init.end());
		}
	}
#if 0
	if (!AST::check_cycle_list(additional_code)) {
		std::cout << "Cycle detected in additional code!" << std::endl;
		exit(1);
	}
#endif

	/* Add ports */
	std::set<std::string> inports;
	std::set<std::string> outports;
	for (auto inp : inst->get_ast()->actor->inports) {
		Connector con;
		con.first = inst->get_name();
		con.second = inp->name.name;
		if (data->new_ports.contains(con)) {
			inports.insert(inp->name.name);
			AST::Port* p = new AST::Port{};
			auto x = data->new_ports[con];
			p->name.name = x;
			p->type = inp->type;
			ast->actor->inports.push_back(p);
		}
	}
	for (auto outp : inst->get_ast()->actor->outports) {
		Connector con;
		con.first = inst->get_name();
		con.second = outp->name.name;
		if (data->new_ports.contains(con)) {
			outports.insert(outp->name.name);
			AST::Port* p = new AST::Port{};
			auto x = data->new_ports[con];
			p->name.name = x;
			p->type = outp->type;
			ast->actor->outports.push_back(p);
		}
	}

#ifdef USE_MUSTPRODUCE
	std::vector<AST::Statement*> must_produce_checks;
	std::vector<AST::Statement*> must_produce_port_checks;
	generate_must_produce_checks(inst, data, must_produce_checks, scm);

	for (auto in : inst->get_in_edges()) {
		if (inports.contains(in->get_dst_port()) && data->must_produce.contains(in)) {
			std::vector<AST::Statement*> res;
			generate_must_produce_input_check(in->get_dst_port(), data->must_produce[in], res);
			must_produce_port_checks.insert(must_produce_port_checks.end(), res.begin(), res.end());
		}
	}
#endif

	std::map<std::string, std::string> functions;
	transform_actions_to_functions(inst, inports, outports, data, composit, functions);

	{
		std::vector<AST::Action*> all_actions = inst->get_ast()->actor->actions;
		if (inst->get_ast()->actor->init != nullptr) {
			all_actions.push_back(inst->get_ast()->actor->init);
		}
		std::set<std::string> renamed_inports;
		std::set<std::string> renamed_outports;
		for (auto oldname : inports) {
			Connector con;
			con.first = inst->get_name();
			con.second = oldname;
			std::string newname = data->new_ports.contains(con) ? data->new_ports[con] : oldname;
			if (newname != oldname) {
				for (auto a : all_actions) {
					rename_port_in_action(a, oldname, newname);
				}
			}
			renamed_inports.insert(newname);
		}
		for (auto oldname : outports) {
			Connector con;
			con.first = inst->get_name();
			con.second = oldname;
			std::string newname = data->new_ports.contains(con) ? data->new_ports[con] : oldname;
			if (newname != oldname) {
				for (auto a : all_actions) {
					rename_port_in_action(a, oldname, newname);
				}
			}
			renamed_outports.insert(newname);
		}
		inports = renamed_inports;
		outports = renamed_outports;
	}

	/* Add state checks to guards */
	if (!init_actions.empty()) {
		assert(has_fsm);
		AST::Action* ast_action = composit->get_init_action();
		AST::Expression* added_guard = new AST::Expression{};

		/* code will be added later */
		AST::IfStatement* block = nullptr;
		AST::IfStatement* prev = nullptr;

		for (auto init_state : init_actions) {
			/* generate state check */
			{
				AST::Expression* e = new AST::Expression{};
				e->brakets = true;
				AST::Operator* ops = new AST::Operator{};
				ops->ops = "==";
				AST::Identifier* ident = new AST::Identifier{};
				ident->identifier = fsmdef->name.name;
				AST::FSM_Enumeration_Element* fe = new AST::FSM_Enumeration_Element{};
				fe->enum_element = init_state.first;
				fe->enum_name = data->enum_map[inst->get_actor()]->name;
				ops->left = ident;
				ops->right = fe;
				e->child = ops;
				if (added_guard->child == nullptr) {
					added_guard->child = e;
				}
				else {
					AST::Operator* ops2 = new AST::Operator{};
					ops2->ops = "||";
					ops2->left = added_guard->child;
					ops2->right = e;
					added_guard->child = ops2;
				}
			}
			/* transform actions and add as if body */
			std::vector<AST::Statement*> res;
			transform_actions(inst, data, init_state.second, inports, outports, init_state.first,
				std::vector<IR::Edge*>(), std::vector<AST::Statement*>(), res, scm, composit, functions);

			/* generate if */
			AST::IfStatement* ifs = new AST::IfStatement{};
			AST::Expression* cond = new AST::Expression{};
			{
				AST::Operator* ops = new AST::Operator{};
				ops->ops = "==";
				AST::Identifier* ident = new AST::Identifier{};
				ident->identifier = fsmdef->name.name;
				AST::FSM_Enumeration_Element* fe = new AST::FSM_Enumeration_Element{};
				fe->enum_element = init_state.first;
				fe->enum_name = data->enum_map[inst->get_actor()]->name;
				ops->left = ident;
				ops->right = fe;
				cond->child = ops;
			}
			ifs->condition = cond;
			for (auto s : res) {
				ifs->ifblock.push_back(s);
			}
			if (block == nullptr) {
				block = ifs;
				prev = ifs;
			}
			else {
				AST::ElseStatement* next = new AST::ElseStatement{};
				next->statements.push_back(ifs);
				prev->elseblock = next;
				prev = ifs;
			}
		}

		std::vector<AST::Statement*> l;
#ifdef USE_MUSTPRODUCE
		if (!must_produce_port_checks.empty()) {
			l.insert(l.begin(), must_produce_port_checks.begin(), must_produce_port_checks.end());
		}
#endif
		l.push_back(block);
		remove_useless_scopes(l);

		scm->statements_init.insert(scm->statements_init.begin(), l.begin(), l.end());
#ifdef USE_MUSTPRODUCE
		for (auto q : must_produce_checks) {
			scm->statements_init.push_back(q);
		}
#endif

		if (!ast_action->guards.empty()) {
			AST::Operator* op = new AST::Operator{};
			op->ops = "||";
			ast_action->guards.front()->brakets = true;
			op->left = ast_action->guards.front();
			op->right = added_guard;
			added_guard->brakets = true;
			AST::Expression* e = new AST::Expression{};
			e->child = op;
			ast_action->guards.clear();
			ast_action->guards.push_back(e);
		}
		else {
			ast_action->guards.push_back(added_guard);
		}
	}

	if (!done_actions.empty()) {
		assert(has_fsm);
		AST::Action* ast_action = composit->get_done_action();
		AST::Expression* added_guard = new AST::Expression{};

		/* code will be added later */
		AST::IfStatement* block = nullptr;
		AST::IfStatement* prev = nullptr;

		for (auto done_state : done_actions) {
			/* generate state check */
			{
				AST::Expression* e = new AST::Expression{};
				e->brakets = true;
				AST::Operator* ops = new AST::Operator{};
				ops->ops = "==";
				AST::Identifier* ident = new AST::Identifier{};
				ident->identifier = fsmdef->name.name;
				AST::FSM_Enumeration_Element* fe = new AST::FSM_Enumeration_Element{};
				fe->enum_element = done_state.first;
				fe->enum_name = data->enum_map[inst->get_actor()]->name;
				ops->left = ident;
				ops->right = fe;
				e->child = ops;
				if (added_guard->child == nullptr) {
					added_guard->child = e;
				}
				else {
					AST::Operator* ops2 = new AST::Operator{};
					ops2->ops = "||";
					ops2->left = added_guard->child;
					ops2->right = e;
					added_guard->child = ops2;
				}
			}
			/* transform actions and add as if body */
			std::vector<AST::Statement*> res;
			transform_actions(inst, data, done_state.second, inports, outports, done_state.first,
				std::vector<IR::Edge*>(), std::vector<AST::Statement*>(), res, scm, composit, functions);

			/* generate if */
			AST::IfStatement* ifs = new AST::IfStatement{};
			AST::Expression* cond = new AST::Expression{};
			{
				AST::Operator* ops = new AST::Operator{};
				ops->ops = "==";
				AST::Identifier* ident = new AST::Identifier{};
				ident->identifier = fsmdef->name.name;
				AST::FSM_Enumeration_Element* fe = new AST::FSM_Enumeration_Element{};
				fe->enum_element = done_state.first;
				fe->enum_name = data->enum_map[inst->get_actor()]->name;
				ops->left = ident;
				ops->right = fe;
				cond->child = ops;
			}
			ifs->condition = cond;
			for (auto s : res) {
				ifs->ifblock.push_back(s);
			}
			if (block == nullptr) {
				block = ifs;
				prev = ifs;
			}
			else {
				AST::ElseStatement* next = new AST::ElseStatement{};
				next->statements.push_back(ifs);
				prev->elseblock = next;
				prev = ifs;
			}
		}

		std::vector<AST::Statement*> l;
#ifdef USE_MUSTPRODUCE
		if (!must_produce_port_checks.empty()) {
			l.insert(l.begin(), must_produce_port_checks.begin(), must_produce_port_checks.end());
		}
#endif
		l.push_back(block);
		remove_useless_scopes(l);

		scm->statements_done.insert(scm->statements_done.begin(), l.begin(), l.end());
#ifdef USE_MUSTPRODUCE
		for (auto q : must_produce_checks) {
			scm->statements_done.push_back(q);
		}
#endif

		if (!ast_action->guards.empty()) {
			AST::Operator* op = new AST::Operator{};
			op->ops = "||";
			ast_action->guards.front()->brakets = true;
			op->left = ast_action->guards.front();
			op->right = added_guard;
			added_guard->brakets = true;
			AST::Expression* e = new AST::Expression{};
			e->child = op;
			ast_action->guards.clear();
			ast_action->guards.push_back(e);
		}
		else {
			ast_action->guards.push_back(added_guard);
		}
	}

	std::vector<AST::Statement*> actual_adds;
	if (!inst->get_actor()->is_static()) {
		actual_adds.insert(actual_adds.begin(), additional_code.begin(), additional_code.end());
	}

	/* No guard for do, it is the "fallback" */
	AST::Statement* block = nullptr;
	AST::IfStatement* prev = nullptr;
	AST::BlockStatement* block_nostate = nullptr;
	AST::ForeachStatement* loop = nullptr;
	/* add loop if necessary */
	if (loopsz > 1) {
		loop = new AST::ForeachStatement{};
		AST::Generator* gen = new AST::Generator{};
		loop->generators.push_back(gen);
		gen->type = new AST::Type{};
		gen->type->type.name = "uint";
		gen->identifier.name = find_unused_name("i", data->used_symbols_do);
		gen->start = new AST::Expression{};
		gen->end = new AST::Expression{};
		AST::Literal* l1 = new AST::Literal{};
		l1->literal = "1";
		AST::Literal* l2 = new AST::Literal{};
		l2->literal = std::to_string(loopsz);
		gen->start->child = l1;
		gen->end->child = l2;

		block = loop;
	}
	if (!has_fsm) {
		std::vector<AST::Statement*> res;
		transform_actions(inst, data, do_actions[""], inports, outports, "", combine_edge, actual_adds, res, scm, composit, functions, true);

		if (loopsz == 1) {
			block_nostate = new AST::BlockStatement{};
			for (auto s : res) {
				block_nostate->statements.push_back(s);
			}
			block = block_nostate;
		}
		else {
			assert(loopsz > 1);
			for (auto s : res) {
				loop->statements.push_back(s);
			}
		}
	}
	else {
		bool first = true;
		for (auto do_state : do_actions) {
			//std::cout << "Transform " << do_state.first << std::endl;
			/* transform actions and add as if body */
			std::vector<AST::Statement*> res;
			transform_actions(inst, data, do_state.second, inports, outports, do_state.first, combine_edge,
				actual_adds, res, scm, composit, functions, true);

			/* generate if */
			AST::IfStatement* ifs = new AST::IfStatement{};
			AST::Expression* cond = new AST::Expression{};
			{
				AST::Operator* ops = new AST::Operator{};
				ops->ops = "==";
				AST::Identifier* ident = new AST::Identifier{};
				ident->identifier = fsmdef->name.name;
				AST::FSM_Enumeration_Element* fe = new AST::FSM_Enumeration_Element{};
				fe->enum_element = do_state.first;
				fe->enum_name = data->enum_map[inst->get_actor()]->name;
				ops->left = ident;
				ops->right = fe;
				cond->child = ops;
			}
			ifs->condition = cond;
			for (auto s : res) {
				ifs->ifblock.push_back(s);
			}
			if (first) {
				first = false;
				prev = ifs;
				if (loopsz > 1) {
					loop->statements.push_back(ifs);
				}
				if (block == nullptr) {
					block = ifs;
				}
			}
			else {
				AST::ElseStatement* next = new AST::ElseStatement{};
				next->statements.push_back(ifs);
				prev->elseblock = next;
				prev = ifs;
			}
		}
	}

#if 0
	if (!AST::check_cycle(block)) {
		std::cout << "Cycle detected in do code!" << std::endl;
		AST::print_statement(block, "");
		exit(1);
	}
#endif
	std::vector<AST::Statement*> l;
#ifdef USE_MUSTPRODUCE
	if (!must_produce_port_checks.empty()) {
		l.insert(l.begin(), must_produce_port_checks.begin(), must_produce_port_checks.end());
	}
#endif
	l.push_back(block);
	if (inst->get_actor()->is_static()) {
		l.insert(l.end(), additional_code.begin(), additional_code.end());
	}
	remove_useless_scopes(l, 10);

	scm->statements_do.insert(scm->statements_do.begin(), l.begin(), l.end());

	for (auto x : scm->sub_merges) {
		scm->overall_free_do.insert(scm->overall_free_do.end(), x.second->overall_free_do.begin(), x.second->overall_free_do.end());
		scm->overall_guards_do.insert(scm->overall_guards_do.end(), x.second->overall_guards_do.begin(), x.second->overall_guards_do.end());
		delete x.second;
	}
}

static bool closes_sched_cond_cycle(
	IR::Actor_Instance* inst,
	IR::Actor_Instance* combine,
	Merge_Data* data)
{
	auto parents = data->sched_cond_fusion[combine]->parent_sched_cond_fusion;

	for (auto p : parents) {
		/* no edge to member of p is allowed! */
		for (auto out : inst->get_out_edges()) {
			auto dst = out->get_sink();
			auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
			if (dst == nullptr) {
				continue;
			}
			if (!data->sched_cond_fusion.contains(dst_inst)) {
				continue;
			}
			unsigned id = data->sched_cond_fusion[dst_inst]->id;
			if (p == id) {
				return true;
			}
		}
	}
	return false;
}

static bool is_single_producer(
	IR::Actor_Instance* prod,
	IR::Actor_Instance* cons)
{
	for (auto in : cons->get_in_edges()) {
		auto src = in->get_source();
		auto src_inst = dynamic_cast<IR::Actor_Instance*>(src);
		if ((src_inst == nullptr) || (src_inst->get_composit_actor() != cons->get_composit_actor())) {
			continue;
		}
		if (src != prod) {
			return false;
		}
	}
	return true;
}

static bool is_single_consumer(
	IR::Actor_Instance* prod,
	IR::Actor_Instance* cons)
{
	for (auto out : prod->get_out_edges()) {
		auto dst = out->get_sink();
		auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
		if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != cons->get_composit_actor())) {
			continue;
		}
		if (dst != cons) {
			return false;
		}
	}
	return true;
}

#if 0
static void add_to_sched_cond(
	IR::Actor_Instance* inst,
	unsigned id,
	Merge_Data* data)
{
	auto s = data->sched_cond_fusion_id[id];
	s->fusioned.push_back(inst);

	for (auto out : inst->get_out_edges()) {
		auto dst = out->get_sink();
		auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
		if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != inst->get_composit_actor())) {
			continue;
		}
		if (data->sched_cond_fusion.contains(dst_inst)) {
			if (data->sched_cond_fusion[dst_inst]->id != s->id) {
				data->sched_cond_fusion[dst_inst]->parent_sched_cond_fusion.push_back(s->id);
			}
		}
	}

	/* Re-calculate channelsizes / check if inf channel has to be propagated and reduced! */
	Config* c = Config::getInstance();
	for (auto out : inst->get_out_edges()) {
		auto dst = out->get_sink();
		auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
		if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != inst->get_composit_actor())) {
			continue;
		}
		if (data->sched_cond_fusion.contains(dst_inst) &&
			(data->sched_cond_fusion[dst_inst]->id == s->id))
		{
			if (data->channels[out].size == c->get_FIFO_size()) {
				unsigned new_sz = 2 * std::max(data->maxprod[out], data->mincons[out]);
				{
					auto chan = data->channels[out];
					chan.size = new_sz;
					AST::Expression* e = chan.channel->arrays.front();
					AST::Literal* l = dynamic_cast<AST::Literal*>(e->child);
					assert(l != nullptr);
					l->literal = std::to_string(chan.size);
				}

				/* update all input channels */
				for (auto in : inst->get_in_edges()) {
					if (is_external_input(in, inst)) {
						continue;
					}
					auto chan = data->channels[in];
					chan.size = c->get_FIFO_size();
					if (!chan.channel->arrays.empty()) {
						AST::Expression* e = chan.channel->arrays.front();
						AST::Literal* l = dynamic_cast<AST::Literal*>(e->child);
						assert(l != nullptr);
						l->literal = std::to_string(chan.size);
					}
					else {
						AST::Expression* e = new AST::Expression{};
						AST::Literal* l = new AST::Literal{};
						l->literal = std::to_string(chan.size);
						e->child = l;
						chan.channel->arrays.push_back(e);
					}
				}
				return;
			}
		}
	}
}
#endif

static void create_new_sched_cond(
	IR::Actor_Instance* inst,
	Merge_Data* data)
{
	sched_cond_merge* s = new sched_cond_merge{};
	s->id = data->sched_conf_fusion_id_counter++;
	s->fusioned.push_back(inst);
	data->sched_cond_fusion_id[s->id] = s;
	data->sched_cond_fusion[inst] = s;

	/* s->parent_sched_cond_fusion is empty because we are merging from the back. */
	/* ... but we have to update the childs of this */
	for (auto out : inst->get_out_edges()) {
		auto dst = out->get_sink();
		auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
		if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != inst->get_composit_actor())) {
			continue;
		}
		if (data->sched_cond_fusion.contains(dst_inst)) {
			data->sched_cond_fusion[dst_inst]->parent_sched_cond_fusion.push_back(s->id);
		}
	}
}

static void combine_sched_cond(
	IR::Actor_Instance* inst,
	Merge_Data* data,
	std::vector<IR::Actor_Instance*> list,
	std::set<IR::Edge*> affected_edges)
{
	sched_cond_merge* s = new sched_cond_merge{};
	s->id = data->sched_conf_fusion_id_counter++;
	s->fusioned.push_back(inst);
	data->sched_cond_fusion_id[s->id] = s;
	data->sched_cond_fusion[inst] = s;

	for (auto l : list) {
		if (!data->sched_cond_fusion.contains(l)) {
			continue;
		}
		auto z = data->sched_cond_fusion[l];
		if (!data->sched_cond_fusion_id.contains(z->id)) {
			continue;
		}
		if (z == s) {
			/* might happen that it was already updated because it is in several merge paths */
			continue;
		}
		data->sched_cond_fusion_id[z->id]->added = true;
		s->sub_merges[l] = z;
		for (auto i : z->fusioned) {
			s->fusioned.push_back(i);
			data->sched_cond_fusion[i] = s;

			for (auto out : i->get_out_edges()) {
				auto dst = out->get_sink();
				auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
				if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != l->get_composit_actor())) {
					continue;
				}
				if (data->sched_cond_fusion.contains(dst_inst)) {
					if (data->sched_cond_fusion[dst_inst]->id != s->id) {
						data->sched_cond_fusion[dst_inst]->parent_sched_cond_fusion.push_back(s->id);
					}
				}
			}
		}

#ifdef USE_MUSTPRODUCE
		for (auto mpc : z->must_produce_check) {
			if (!affected_edges.contains(mpc.first)) {
				s->must_produce_check.insert(mpc);
			}
		}
#endif
	}
	
	for (auto out : inst->get_out_edges()) {
		auto dst = out->get_sink();
		auto dst_inst = dynamic_cast<IR::Actor_Instance*>(dst);
		if ((dst_inst == nullptr) || (dst_inst->get_composit_actor() != inst->get_composit_actor())) {
			continue;
		}
		if (data->sched_cond_fusion.contains(dst_inst)) {
			if (data->sched_cond_fusion[dst_inst]->id != s->id) {
				data->sched_cond_fusion[dst_inst]->parent_sched_cond_fusion.push_back(s->id);
			}
		}
	}

	for (auto out : affected_edges) {
		unsigned old_channel_sz = data->channels[out].size;
		unsigned channel_sz = std::max(data->maxprod[out], data->mincons[out]);

		if (old_channel_sz <= channel_sz) {
			continue;
		}
		data->channels[out].size = channel_sz;
		std::set<std::string> contrib;
		for (auto a : inst->get_actor()->get_actions()) {
			bool produces = false;
			for (auto o : a->get_out_buffers()) {
				if ((o.buffer_name == out->get_src_port()) && (o.tokenrate != 0)) {
					produces = true;
				}
			}
			if (!produces) {
				continue;
			}
			for (auto in : a->get_in_buffers()) {
				if (in.tokenrate != 0) {
					contrib.insert(in.buffer_name);
				}
			}
		}

		unsigned prodrate = data->maxprod[out];
		for (auto in : inst->get_in_edges()) {
			if (is_external_input(in, inst)) {
				continue;
			}
			if (!contrib.contains(in->get_dst_port())) {
				continue;
			}
			unsigned consrate = data->maxcons[in];
			unsigned propagated = (prodrate != 0) ? (old_channel_sz * consrate / prodrate) : old_channel_sz;
			if (data->channels[in].size < propagated) {
				data->channels[in].size = propagated;
			}
		}
	}
}

#ifdef USE_MUSTPRODUCE
static bool edge_is_must_produce(
	IR::Edge* edge,
	Merge_Data* data)
{
	if (!data->must_produce.contains(edge)) {
		return false;
	}
	if (data->must_produce[edge] < data->maxcons[edge]) {
		return false;
	}
	return true;
}
#endif

static void find_combine_actor(
	IR::Actor_Instance* instance,
	Merge_Data* merge_data,
	std::vector<IR::Actor_Instance*> &result)
{
	std::set<IR::Actor_Instance*> res_tmp;

	std::set<IR::Edge*> affected_edges;

	for (auto out : instance->get_out_edges()) {
		if (out->get_feedback()) {
			continue;
		}
		auto x = dynamic_cast<IR::Actor_Instance*>(out->get_sink());
		if ((x == nullptr) || (instance->get_composit_actor() != x->get_composit_actor())) {
			continue;
		}
		bool recursion = closes_sched_cond_cycle(instance, x, merge_data);
		if (recursion
#ifdef USE_MUSTPRODUCE
			|| edge_is_must_produce(out, merge_data)
#endif
			) {
			continue;
		}
		
		unsigned prod_loop = merge_data->loopcount[instance];
		unsigned cons_loop = merge_data->loopcount[dynamic_cast<IR::Actor_Instance*>(out->get_sink())];
		if (((merge_data->minprod[out] * prod_loop) == (merge_data->maxcons[out] * cons_loop)) &&
			((merge_data->maxprod[out] * prod_loop) == (merge_data->mincons[out] * cons_loop)) &&
			(is_single_consumer(instance, x) || is_single_producer(instance, x) || x->get_actor()->is_static()))
		{
			res_tmp.insert(x);
			affected_edges.insert(out);
		}
	}
	
	for (auto x : res_tmp) {
		result.push_back(x);
	}

	if (result.empty()) {
		create_new_sched_cond(instance, merge_data);
	}
	else {
		combine_sched_cond(instance, merge_data, result, affected_edges);
	}
}

static void add_parameters_variables_functions(
	IR::Dataflow_Network *dpn,
	IR::Actor_Instance* inst,
	IR::Composit_Actor* composit,
	std::set<std::string>& used_symbols,
	std::map<std::string, std::string>& replacements)
{
	AST::Actor* ast = inst->get_ast()->actor;
	AST::Actor* target = composit->get_ast()->actor;
	std::map<std::string, std::string> inst_params;
	dpn->get_params_for_instance(inst->get_name(), inst_params);
	for (auto param : ast->parameters) {
		AST::VarDefinition* v = new AST::VarDefinition{};
		if (used_symbols.contains(param->name.name)) {
			v->name.name = find_unused_name(param->name.name, used_symbols);
			replacements[param->name.name] = v->name.name;
		}
		else {
			v->name.name = param->name.name;
		}
		used_symbols.insert(v->name.name);
		v->type = param->type;
		if (inst_params.contains(param->name.name)) {
			AST::Expression* e = new AST::Expression{};
			AST::Literal* l = new AST::Literal{};
			l->literal = inst_params[param->name.name];
			e->child = l;
			v->assign = e;
			v->constassign = true;
		}
		else {
			AST::Expression* e = new AST::Expression{ *param->asign };
			v->constassign = true;
			v->assign = e;
			AST_Transform::replace_identifiers_expr(e, replacements);
		}
		target->vars.push_back(v);
	}
	for (auto var : ast->vars) {
		AST::VarDefinition* v = new AST::VarDefinition{ *var };
		if (used_symbols.contains(var->name.name)) {
			v->name.name = find_unused_name(var->name.name, used_symbols);
			replacements[var->name.name] = v->name.name;
		}
		used_symbols.insert(v->name.name);
		target->vars.push_back(v);
		if (v->assign != nullptr) {
			AST_Transform::replace_identifiers_expr(v->assign, replacements);
		}
	}
	for (auto function : ast->functions) {
		AST::Function* f = new AST::Function{ *function };
		if (used_symbols.contains(function->name.name)) {
			f->name.name = find_unused_name(f->name.name, used_symbols);
			replacements[function->name.name] = f->name.name;
		}
		used_symbols.insert(f->name.name);
		target->functions.push_back(f);
		AST_Transform::replace_identifiers_expr(f->expression, replacements);
	}
	for (auto procedure : ast->procedures) {
		AST::Procedure* p = new AST::Procedure{ *procedure };
		if (used_symbols.contains(procedure->name.name)) {
			p->name.name = find_unused_name(procedure->name.name, used_symbols);
			replacements[procedure->name.name] = p->name.name;
		}
		used_symbols.insert(p->name.name);
		target->procedures.push_back(p);
		AST_Transform::replace_identifiers_vardef(p->vars, replacements);
		AST_Transform::replace_identifiers_stmt(p->statements, replacements);
	}
	/* Assuming native functions are the same everywhere, hence, we don't rename them */
	for (auto native : ast->nativefunctions) {
		if (!used_symbols.contains(native->name.name)) {
			AST::NativeFunction* n = new AST::NativeFunction{ *native };
			target->nativefunctions.push_back(n);
			used_symbols.insert(n->name.name);
		}
	}
	for (auto native : ast->nativeprocedures) {
		if (!used_symbols.contains(native->name.name)) {
			AST::NativeProcedure* n = new AST::NativeProcedure{ *native };
			target->nativeprocedures.push_back(n);
			used_symbols.insert(n->name.name);
		}
	}
}

static void add_imports(
	IR::Actor_Instance* inst,
	IR::Composit_Actor* composit,
	std::vector<IR::Unit*>& added_imports)
{
	for (auto import : inst->get_actor()->get_imported_symbols()) {
		if (std::find(added_imports.begin(), added_imports.end(), import) == added_imports.end()) {
			added_imports.push_back(import);
			composit->add_imported_symbol(import);
		}
	}
}

static void add_initialize_action(
	IR::Actor_Instance *inst,
	IR::Composit_Actor* composit,
	Merge_Data* data,
	AST::Action* initalize,
	std::set<std::string>& used_symbols,
	std::map<std::string, std::string> replacements)
{
	auto ast = composit->get_initialize_action();
	for (auto var : initalize->vars) {
		AST::VarDefinition* v = new AST::VarDefinition{ *var };
		if (used_symbols.contains(v->name.name)) {
			v->name.name = find_unused_name(v->name.name, used_symbols);
		}
		replacements[var->name.name] = v->name.name;
		if (v->assign != nullptr) {
			AST_Transform::replace_identifiers_expr(v->assign, replacements);
		}
		for (auto array : v->arrays) {
			AST_Transform::replace_identifiers_expr(array, replacements);
		}
		used_symbols.insert(v->name.name);
		ast->vars.push_back(v);
	}
	std::vector<AST::Statement*> statements;
	for (auto stmt : initalize->statements) {
		AST::Statement* s = stmt->clone();
		statements.push_back(s);
	}

	AST_Transform::replace_identifiers_stmt(statements, replacements);

	AST::Action tmp_action;
	tmp_action.statements = statements;
	for (auto edge : inst->get_out_edges()) {
		Connector con;
		con.first = inst->get_name();
		con.second = edge->get_src_port();
		if (!data->new_ports.contains(con)) {
			auto channel = data->channels[edge];
			std::string index_r_name;
			if (channel.index_r != nullptr) {
				index_r_name = channel.index_r->name.name;
			}
			while (true) {
				std::vector<AST::Statement*>* stmt_list = nullptr;
				auto o = find_port_write(&tmp_action, edge->get_src_port(), &stmt_list);
				if (o == nullptr) {
					break;
				}
				assert(stmt_list != nullptr);
				AST_Transform::channelwritestmt_assignment(&tmp_action, o, channel.channel->name.name,
					index_r_name, channel.sz->name.name, channel.size, stmt_list);
			}
		}
	}

	ast->statements.insert(ast->statements.end(), tmp_action.statements.begin(), tmp_action.statements.end());
}

void add_to_merge(
	Merge_Data* merge_data,
	IR::Dataflow_Network *dpn,
	IR::Actor_Instance *instance,
	IR::Composit_Actor *composit,
	std::map<std::string, std::string>& replacements)
{
	AST::AST_Root* ast = instance->get_ast();

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Combing " << instance->get_name() << std::endl;
#endif

	AST::VarDefinition* fsmvardef = nullptr;
	/* Check if FSM has to be transformed */
	if (ast->actor->fsm != nullptr) {
		AST::FSM_Enumeration* fsmenum = get_fsmenum(instance, merge_data, composit);
		fsmvardef = new AST::VarDefinition{};
		fsmvardef->constassign = false;
		fsmvardef->name.name = find_unused_name(instance->get_name() + "_state", merge_data->used_symbols);
		fsmvardef->type.type.name = fsmenum->name;
		fsmvardef->type.non_standard_type = true;
		fsmvardef->assign = new AST::Expression{};
		AST::Literal *l = new AST::Literal{};
		fsmvardef->assign->child = l;
		l->literal = fsmenum->inital_state;
		composit->get_ast()->actor->vars.push_back(fsmvardef);
		merge_data->fsmvar[instance] = fsmvardef->name.name;
	}

	std::vector<IR::Actor_Instance*>& combine = merge_data->combine_actors[instance];
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Combine actors:";
	for (auto c : combine) {
		std::cout << " " << c->get_name();
	}
	std::cout << std::endl;
	sched_cond_merge* scm = merge_data->scm[instance];
	std::cout << "All actors included:";
	for (auto x : scm->fusioned) {
		std::cout << " " << x->get_name();
	}
	std::cout << std::endl;
#endif

	/* Perform the merge */
	combine_actions(composit, instance, fsmvardef, merge_data, replacements, combine);
}

static void determine_edge_relations(
	std::vector<IR::Actor_Instance*> cluster,
	IR::Dataflow_Network* dpn,
	Merge_Data* data)
{
	for (auto inst : cluster) {
		std::map<std::string, IR::Edge*> port_edge_map;
		std::map<IR::Edge*, unsigned> mp_out;
		for (auto in : inst->get_in_edges()) {
			port_edge_map[in->get_dst_port()] = in;
		}
		for (auto out : inst->get_out_edges()) {
			port_edge_map[out->get_src_port()] = out;
			mp_out[out] = 999999;
		}

		/* first do propagation */
		for (auto in : inst->get_in_edges()) {
			unsigned highest_cons = 0;
			std::map<IR::Edge*, unsigned> mp = mp_out;

			for (auto a : inst->get_actor()->get_actions()) {
				bool not_covered = false;
				for (auto t : a->get_in_buffers()) {
					if ((t.buffer_name == in->get_dst_port()) && (t.tokenrate == 0)) {
						not_covered = true;
					}
					if (t.buffer_name == in->get_dst_port()) {
						if (t.tokenrate > highest_cons) {
							highest_cons = t.tokenrate;
						}
					}
				}
				if (not_covered) {
					continue;
				}
				for (auto out : a->get_out_buffers()) {
					auto edge = port_edge_map[out.buffer_name];
					if (out.tokenrate < mp[edge]) {
						mp[edge] = out.tokenrate;
					}
				}
			}

			for (auto m : mp) {
				if ((m.second != 0) && (m.second != 999999)) {
					std::vector<Channel_Dependency> rel;
					if (data->edge_relations.contains(in)) {
						for (auto x : data->edge_relations[in]) {
							if (highest_cons != 0) {
								x.value = x.value * m.second / highest_cons;
							}
							rel.push_back(x);
						}
						data->edge_relations[m.first] = rel;
					}
				}
			}
		}

		/* next do add of new dependencies */
		for (auto out : inst->get_out_edges()) {
			std::map<IR::Edge*, unsigned> mp = mp_out;
			for (auto a : inst->get_actor()->get_actions()) {
				bool not_covered = false;
				for (auto t : a->get_out_buffers()) {
					if ((t.buffer_name == out->get_src_port()) && (t.tokenrate == 0)) {
						not_covered = true;
					}
				}
				if (not_covered) {
					continue;
				}
				for (auto out : a->get_out_buffers()) {
					auto edge = port_edge_map[out.buffer_name];
					if (mp[edge] > out.tokenrate) {
						mp[edge] = out.tokenrate;
					}
				}
			}

			/* now the dependencies for the edge currently under evaluation */
			std::vector<IR::Edge*> other_edges;
			for (auto o : inst->get_out_edges()) {
				if ((o != out) && (mp[o] != 0) && (mp[o] != 999999)) {
					other_edges.push_back(o);
				}
			}
			for (auto o : other_edges) {
				Channel_Dependency c;
				c.edge = o;
				c.value = (mp[out] != 0) ? static_cast<double>(mp[o]) / mp[out] : 0.0;
				if (data->edge_relations.contains(out)) {
					data->edge_relations[out].push_back(c);
				}
				else {
					std::vector<Channel_Dependency> rel;
					rel.push_back(c);
					data->edge_relations[out] = rel;
				}
			}
		}
	}
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Edge relations:\n";
	for (auto x : data->edge_relations) {
		std::cout << x.first->get_name() << " --> (";
		for (auto y : x.second) {
			std::cout << "(" << y.edge->get_name() << ", " << y.value << ")";
		}
		std::cout << std::endl;
	}
#endif
}

#ifdef USE_MUSTPRODUCE
#define MUST_PRODUCE_INVAL 99999
static void determine_must_produce(
	std::vector<IR::Actor_Instance*> cluster,
	std::set<IR::Actor_Instance*> outputs,
	IR::Dataflow_Network* dpn,
	Merge_Data* data)
{
	/* More of less for future use, only detect the all static case. */
	bool all_static = true;
	for (auto inst : cluster) {
		all_static &= inst->get_actor()->is_static();
	}
	data->all_static = all_static;

	std::set<IR::Actor_Instance*> current_list;
	/* first sort out the outputs that are also producing internal tokens */
	for (auto x : outputs) {
		bool all = true;

		for (auto o : x->get_out_edges()) {
			if (o->get_feedback()) {
				continue;
			}
			IR::Actor_Instance_Base* b = o->get_sink();
			if (dynamic_cast<IR::Actor_Instance*>(b) != nullptr) {
				auto q = dynamic_cast<IR::Actor_Instance*>(b);
				if (q->get_composit_actor() == x->get_composit_actor()) {
					all = false;
				}
			}
			else {
				auto q = dynamic_cast<IR::Composit_Actor*>(b);
				if (q == x->get_composit_actor()) {
					all = false;
				}
			}
		}

		if (all) {
			current_list.insert(x);
		}
	}

	/* a list of all actor instances that produce must produce edges but don't receive them */
	std::vector<IR::Actor_Instance*> actors_to_check;

	for (auto rit = cluster.rbegin(); rit != cluster.rend(); ++rit) {
		if (current_list.contains(*rit)) {
			for (auto in : (*rit)->get_in_edges()) {
				if (data->mincons[in] != 0) {
					data->must_produce[in] = data->mincons[in];
				}
			}
		}
		else {
			std::map<std::string, IR::Edge*> port_edge_map;
			for (auto in : (*rit)->get_in_edges()) {
				port_edge_map[in->get_dst_port()] = in;
			}

			std::map<std::string, unsigned> mp_req;
			for (auto out : (*rit)->get_out_edges()) {
				if (data->must_produce.contains(out) && (data->must_produce[out] != 0)) {
					mp_req[out->get_src_port()] = data->must_produce[out]; 
				}
			}
			if (mp_req.empty()) {
				continue;
			}
			actors_to_check.push_back(*rit);
			std::map<IR::Edge*, unsigned> mp_val;
			for (auto action : (*rit)->get_actor()->get_actions()) {
				bool all_covered = true;

				std::map<std::string, unsigned> covered;
				for (auto out : action->get_out_buffers()) {
					if (mp_req.contains(out.buffer_name)) {
						if (out.tokenrate < mp_req[out.buffer_name]) {
							all_covered = false;
						}
						else {
							covered[out.buffer_name] = out.tokenrate;
						}
					}
				}

				if (covered.empty()) {
					continue;
				}

				std::map<std::string, unsigned> required;
				for (auto in : action->get_in_buffers()) {
					if (in.tokenrate != 0) {
						required[in.buffer_name] = in.tokenrate;
					}
				}

				if ((covered.size() == 1) || (*rit)->get_actor()->is_static()) {
					for (auto r : required) {
						auto edge = port_edge_map[r.first];
						if (mp_val.contains(edge)) {
							if (mp_val[edge] > r.second) {
								mp_val[edge] = r.second;
							}
						}
						else {
							mp_val[edge] = r.second;
						}
					}
				}
				else {
					/* This requires is_compoensated checkes, if not compoensated ignore this, otherwise all lower value. 
					 * Leave this for the future because it is not relevant for now.
					 */
					std::cout << "Must produce check does not implement this case." << std::endl;
					exit(1);
				}
			}

			for (auto x : mp_val) {
				if ((x.second != 0) && (x.second != MUST_PRODUCE_INVAL)) {
					data->must_produce[x.first] = x.second;
				}
			}
		}
	}


	/* now do backprop to erase all edges that are not must produce */
	for (auto it : actors_to_check) {
		bool predecessor = true;
		for (auto o : current_list) {
			predecessor &= o->is_predecessor(it);
		}
		if (!predecessor) {
			/* do further checks here */
			std::map<IR::Edge*, unsigned> mp;
			std::map<std::string, IR::Edge*> port_edge_map;
			for (auto in : it->get_in_edges()) {
				port_edge_map[in->get_dst_port()] = in;
			}
			for (auto out : it->get_out_edges()) {
				mp[out] = MUST_PRODUCE_INVAL;
				port_edge_map[out->get_src_port()] = out;
			}
			for (auto a : it->get_actor()->get_actions()) {
				std::map<IR::Edge*, unsigned> mp_edge;
				bool has_in_must_edge = false;
				for (auto in : a->get_in_buffers()) {
					auto edge = port_edge_map[in.buffer_name];
					if (data->must_produce.contains(edge)) {
						has_in_must_edge = true;
					}
				}
				if (has_in_must_edge) {
					for (auto out : a->get_out_buffers()) {
						auto edge = port_edge_map[out.buffer_name];
						unsigned tokenrate = data->must_produce[edge];
						if (mp[edge] > tokenrate) {
							mp[edge] = tokenrate;
						}
					}
				}
			}

			for (auto out : it->get_out_edges()) {
				if (((mp[out] == 0) || (mp[out] == MUST_PRODUCE_INVAL)) && data->must_produce.contains(out)) {
					data->must_produce.erase(out);
					auto q = dynamic_cast<IR::Actor_Instance*>(out->get_sink());
					if ((q != nullptr) && (q->get_composit_actor() == it->get_composit_actor()) &&
						std::find(actors_to_check.begin(), actors_to_check.end(), q) == actors_to_check.end())
					{
						actors_to_check.push_back(q);
					}
				}
			}
		}
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Must Produce:";
	for (auto x : data->must_produce) {
		std::cout << " (" << x.first->get_name() << ", " << x.second << ")";
	}
	std::cout << std::endl;
#endif
}
#endif


#define LOOPSZ_INVAL 99999
static unsigned calculate_loopsz(
	IR::Actor_Instance* inst,
	Merge_Data* data,
	bool backprop,
	bool max)
{
	unsigned result = LOOPSZ_INVAL;
	if (backprop) {
		for (auto out : inst->get_out_edges()) {
			if (out->get_feedback()) {
				continue;
			}
			auto dst = dynamic_cast<IR::Actor_Instance*>(out->get_sink());
			if ((dst != nullptr) && (inst->get_composit_actor() == dst->get_composit_actor())) {
				unsigned x;
				if (max) {
					x = data->mincons[out] * data->max_loopcount[dst];
				}
				else {
					x = data->maxcons[out] * data->loopcount[dst];
				}
				unsigned y;
#ifdef USE_MUSTPRODUCE
				if (data->must_produce.contains(out)) {
					y = data->must_produce[out];
				}
				else {
#endif
					if (max) {
						y = data->maxprod[out];
					}
					else {
						y = data->maxprod[out] + data->minprod[out];
						y = y / 2;
					}
					if (y == 0) {
						y = 1; /* division by zero is bad ... */
					}
#ifdef USE_MUSTPRODUCE
				}
#endif

				x = x / y;

				if (max) {
					if ((x > result) || (result == LOOPSZ_INVAL)) {
						result = x;
					}
				}
				else {
					if (x < result) {
						result = x;
					}
				}
			}
		}
	}
	else {
		for (auto in : inst->get_in_edges()) {
			if (in->get_feedback()) {
				continue;
			}
			auto src = dynamic_cast<IR::Actor_Instance*>(in->get_source());
			if ((src != nullptr) && (inst->get_composit_actor() == src->get_composit_actor())) {
				unsigned x = 0;
				if (max) {
					unsigned d = (data->mincons[in] != 0) ? data->mincons[in] : data->maxcons[in];
					if (d == 0) {
						continue;
					}
					x = data->maxprod[in] * data->max_loopcount[src] / d;
				}
				else {
					if (data->maxcons[in] == 0) {
						/* consumer never consumes from this edge -> no constraint */
						continue;
					}
					x = data->minprod[in] * data->loopcount[src] / data->maxcons[in];
				}
#ifdef USE_MUSTPRODUCE
				unsigned guaranteed = 0;
				if (data->must_produce.contains(in)) {
					if (max) {
						if (data->mincons[in] > 0) {
							guaranteed = data->must_produce[in] / data->mincons[in];
						}
					}
					else {
						guaranteed = data->must_produce[in] / data->maxcons[in];
					}
				}
				if (guaranteed > x) {
					x = guaranteed;
				}
#endif
				if (max) {
					if ((x > result) || (result == LOOPSZ_INVAL)) {
						result = x;
					}
				}
				else {
					if (x < result) {
						result = x;
					}
				}
			}
		}
	}
	if ((result == LOOPSZ_INVAL) || (result == 0)) {
		/* source actors don't update this, will be fixed by backprop */
		result = 1;
	}
	return result;
}

static void do_backpropagation(
	IR::Actor_Instance* start,
	Merge_Data* data,
	std::vector<IR::Actor_Instance*>& cluster,
	bool max)
{
	std::vector<IR::Actor_Instance*> process_list;
	for (auto in : start->get_in_edges()) {
		if (in->get_feedback()) {
			continue;
		}
		auto src = dynamic_cast<IR::Actor_Instance*>(in->get_source());
		if ((src != nullptr) && (start->get_composit_actor() == src->get_composit_actor())) {
			process_list.push_back(src);
		}
	}

	while (!process_list.empty()) {
		auto i = process_list.begin();
		auto inst = *i;
		process_list.erase(i);
		unsigned x = calculate_loopsz(inst, data, true, max);
		if (max && (x > data->max_loopcount[inst])) {
			data->max_loopcount[inst] = x;
		}
		else if (!max && (x != data->loopcount[inst])) {
			data->loopcount[inst] = x;
		}
		else {
			continue;
		}

		for (auto in : inst->get_in_edges()) {
			if (in->get_feedback()) {
				continue;
			}
			auto src = dynamic_cast<IR::Actor_Instance*>(in->get_source());
			if ((src != nullptr) && (start->get_composit_actor() == src->get_composit_actor())) {
				process_list.push_back(src);
			}
		}
	}
}

static void determine_loopcount(
	Merge_Data* data,
	std::vector<IR::Actor_Instance*> cluster)
{
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Start loop count detection" << std::endl;
	for (auto x : data->mincons) {
		std::cout << "mincons: " << x.first->get_name() << ": " << x.second << std::endl;
 	}
	for (auto x : data->maxcons) {
		std::cout << "maxcons: " << x.first->get_name() << ": " << x.second << std::endl;
	}
	for (auto x : data->minprod) {
		std::cout << "minprod: " << x.first->get_name() << ": " << x.second << std::endl;
	}
	for (auto x : data->maxprod) {
		std::cout << "maxprod: " << x.first->get_name() << ": " << x.second << std::endl;
	}
#endif

	for (auto inst : cluster) {
		unsigned l = calculate_loopsz(inst, data, false, false);
		if (l != data->loopcount[inst]) {
			data->loopcount[inst] = l;
			do_backpropagation(inst, data, cluster, false);
		}
	}

	/* Now adjust channelsizes */
	for (auto inst : cluster) {
		unsigned loopsz = data->loopcount[inst];
		if (loopsz == 1) {
			continue;
		}
		for (auto out : inst->get_out_edges()) {
			if (!data->channels.contains(out)) {
				/* must be connection to another cluster/actor outside this cluster */
				continue;
			}
			data->channels[out].size *= loopsz;
		}
	}
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Loopcount:";
	for (auto x : data->loopcount) {
		std::cout << " (" << x.first->get_name() << ", " << x.second << ")";
	}
	std::cout << std::endl;
	std::cout << "Adjusted Channel sizes:";
	for (auto x : data->channels) {
		std::cout << " (" << x.first->get_name() << ", " << x.second.size << ")";
	}
	std::cout << std::endl;
#endif
}

static void determine_max_loopcount(
	std::vector<IR::Actor_Instance*> cluster,
	Merge_Data* data)
{
	for (auto inst : cluster) {
		unsigned l = calculate_loopsz(inst, data, false, true);
		if (l != data->max_loopcount[inst]) {
			data->max_loopcount[inst] = l;
			do_backpropagation(inst, data, cluster, true);
		}
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "MaxLoopcount:";
	for (auto x : data->max_loopcount) {
		std::cout << " (" << x.first->get_name() << ", " << x.second << ")";
	}
	std::cout << std::endl;
#endif
}

static void determine_guaranteed_production(
	std::vector<IR::Actor_Instance*> cluster,
	Merge_Data* data)
{
	Config* c = Config::getInstance();

	for (auto inst : cluster) {
		/* Ignore channels that are input! */
		std::set<IR::Edge*> inputs;
		std::set<IR::Edge*> outputs;
		std::map<std::string, IR::Edge*> port_edge_map;
		std::map<IR::Edge*, unsigned> edge_size_map;
		for (auto inedge : inst->get_in_edges()) {
			port_edge_map[inedge->get_dst_port()] = inedge;
			auto b = inedge->get_source();
			if (dynamic_cast<IR::Actor_Instance*>(b) != nullptr) {
				auto i = dynamic_cast<IR::Actor_Instance*>(b);
				if (i->get_composit_actor() != inst->get_composit_actor()) {
					inputs.insert(inedge);
				}
			}
			else {
				inputs.insert(inedge);
			}
		}

		std::set<std::string> inf_inputs;
		for (auto in : inst->get_in_edges()) {
			if (data->guaranteed_production.contains(in) && (data->guaranteed_production[in] == 0)) {
				inf_inputs.insert(in->get_dst_port());
			}
			else if (inputs.contains(in)) {
				inf_inputs.insert(in->get_dst_port());
			}
		}

		std::set<IR::Edge*> out_edges;
		std::map<std::string, IR::Edge*> out_port_edge_map;
		for (auto out : inst->get_out_edges()) {
			out_edges.insert(out);
			out_port_edge_map[out->get_src_port()] = out;

			auto b = out->get_sink();
			if (dynamic_cast<IR::Actor_Instance*>(b) != nullptr) {
				auto i = dynamic_cast<IR::Actor_Instance*>(b);
				if (i->get_composit_actor() != inst->get_composit_actor()) {
					outputs.insert(out);
				}
			}
			else {
				outputs.insert(out);
			}
		}

		std::set<IR::Edge*> channels_to_adjust;

		if (inst->get_in_edges().size() > 1) {
			std::set<std::string> states;
			inst->get_actor()->get_all_states(states);
			for (auto state : states) {
				std::set<std::string> nocompensation;
				for (auto action : inst->get_actor()->find_schedulable_actions(state)) {
					std::set<std::string> other_channels;
					bool produces_for_port = false;
					for (auto out : action->get_out_buffers()) {
						if (out.tokenrate == 0) {
							continue;
						}
						if (outputs.contains(out_port_edge_map[out.buffer_name])) {
							produces_for_port = true;
						}
					}
					for (auto in : action->get_in_buffers()) {
						if (in.tokenrate == 0) {
							continue;
						}
						auto edge = port_edge_map[in.buffer_name];
						if (!inf_inputs.contains(in.buffer_name)) {
							other_channels.insert(in.buffer_name);
						}
					}

					std::set<std::string> tmp;
					bool v = is_relation_compensated(port_edge_map, action, data, inf_inputs, tmp);
					if (!v) {
						if (!other_channels.empty()) {
							for (auto out : action->get_out_buffers()) {
								if (out.tokenrate == 0) {
									continue;
								}
								if (out_edges.contains(out_port_edge_map[out.buffer_name])) {
									out_edges.erase(out_port_edge_map[out.buffer_name]);
								}
							}
							is_underflow_compensated(inst->get_actor(), tmp, state, other_channels, nocompensation);
						}
					}

					if (produces_for_port) {
						/* Channels dependent on outputs must be of inf size because the production depends on outside conditions
						 * hence, the production here might stall.
						 */
						for (auto in : action->get_in_buffers()) {
							if (in.tokenrate == 0) {
								continue;
							}
							auto edge = port_edge_map[in.buffer_name];
							channels_to_adjust.insert(edge);
						}
					}
				}
				for (auto n : nocompensation) {
					channels_to_adjust.insert(port_edge_map[n]);
				}
			}
		}
		else {
			/* Single In, nothing to change. */
		}

		for (auto out : inst->get_out_edges()) {
			if (out_edges.contains(out)
#ifdef USE_MUSTPRODUCE
				&& !data->must_produce.contains(out)
#endif
			) {
				data->guaranteed_production[out] = 0;
			}
			else {
				data->guaranteed_production[out] = 1;
			}
		}

		/* now adjust channel sizes based on guarantee */
		for (auto in : channels_to_adjust) {
			if (inputs.contains(in)) {
				continue;
			}
			data->channels[in].size = (in->get_specified_size() != 0) ? in->get_specified_size() : c->get_FIFO_size();
		}
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Adjusted Channel sizes based on guarantees:";
	for (auto x : data->channels) {
		std::cout << " (" << x.first->get_name() << ", " << x.second.size << ")";
	}
	std::cout << std::endl;
#endif
}

static void get_do_code(
	sched_cond_merge* scm,
	std::vector<AST::Statement*>& stmts)
{
	AST::IfStatement* prev_if = nullptr;
	if (!scm->overall_guards_do.empty()) {
		std::vector<AST::Expression*> exprs;
		for (auto i : scm->overall_guards_do) {
			auto g = dynamic_cast<AST::Literal*>(i->child);
			if ((g != nullptr) && (g->literal == "true")) {
				continue;
			}
			exprs.push_back(i);
		}
		if (!exprs.empty()) {
			AST::IfStatement* ifs = new AST::IfStatement{};
			stmts.push_back(ifs);
			prev_if = ifs;

			if (exprs.size() == 1) {
				ifs->condition = exprs.front();
			}
			else {
				AST::Expression* cond = new AST::Expression{};
				ifs->condition = cond;

				AST::Operator* ops = new AST::Operator{};
				cond->child = ops;
				ops->ops = "&&";
				ops->left = exprs.front();
				exprs.front()->brakets = true;
				for (unsigned it = 1; it < (exprs.size() - 1); ++it) {
					AST::Operator* o = new AST::Operator{};
					o->ops = "&&";
					ops->right = o;
					o->left = exprs.at(it);
					exprs.at(it)->brakets = true;
					ops = o;
				}
				ops->right = exprs.back();
				exprs.back()->brakets = true;
			}
		}
	}
	if (!scm->overall_free_do.empty()) {
		std::vector<AST::Expression*> exprs;
		for (auto i : scm->overall_free_do) {
			auto g = dynamic_cast<AST::Literal*>(i->child);
			if ((g != nullptr) && (g->literal == "true")) {
				continue;
			}
			exprs.push_back(i);
		}
		if (!exprs.empty()) {
			AST::IfStatement* ifs = new AST::IfStatement{};
			if (prev_if == nullptr) {
				stmts.push_back(ifs);
			}
			else {
				prev_if->ifblock.push_back(ifs);
			}
			prev_if = ifs;

			if (exprs.size() == 1) {
				ifs->condition = exprs.front();
			}
			else {
				AST::Expression* cond = new AST::Expression{};
				ifs->condition = cond;

				AST::Operator* ops = new AST::Operator{};
				cond->child = ops;
				ops->ops = "&&";
				ops->left = exprs.front();
				exprs.front()->brakets = true;
				for (unsigned it = 1; it < (exprs.size() - 1); ++it) {
					AST::Operator* o = new AST::Operator{};
					o->ops = "&&";
					ops->right = o;
					o->left = exprs.at(it);
					exprs.at(it)->brakets = true;
					ops = o;
				}
				ops->right = exprs.back();
				exprs.back()->brakets = true;
			}
		}
	}
	if (prev_if == nullptr) {
		stmts.insert(stmts.begin(), scm->statements_do.begin(), scm->statements_do.end());
	}
	else {
		prev_if->ifblock.insert(prev_if->ifblock.begin(), scm->statements_do.begin(), scm->statements_do.end());
	}
#ifdef USE_MUSTPRODUCE
	for (auto mp : scm->must_produce_check) {
		stmts.push_back(mp.second);
	}
#endif
}

static void create_channel_vars(
	IR::Composit_Actor* composit,
	Merge_Data* merge_data)
{
	AST::Actor* ast = composit->get_ast()->actor;
	for (auto& entry : merge_data->channels) {
		Channel_Variable& cv = entry.second;
		create_channel(cv, entry.first);
		ast->vars.push_back(cv.channel);
		if (cv.index_r != nullptr) {
			ast->vars.push_back(cv.index_r);
		}
		ast->vars.push_back(cv.sz);
	}
}

static void perform_merge(
	unsigned core,
	std::vector<IR::Actor_Instance*> cluster,
	IR::Dataflow_Network* dpn)
{
	if (cluster.size() <= 1) {
		/* nothing to do, a single node or no node cannot be merged! */
		return;
	}

	std::string composit_name = "merge_" + std::to_string(cluster.front()->get_cluster_id());
	IR::Composit_Actor* composit = new IR::Composit_Actor{composit_name,
														  cluster.front()->get_cluster_id(), core };
	Merge_Data* merge_data = new Merge_Data{};
	dpn->add_composit_actor(composit);

	for (auto inst = cluster.begin(); inst != cluster.end(); ++inst) {
		(*inst)->set_composit_actor(composit);
		(*inst)->set_deleted();
	}

	std::set<std::string> used_ports;
	std::set<IR::Actor_Instance*> outputs;
	for (auto inst: cluster) {
		if (inst->get_source()) {
			composit->set_source();
		}
		if (inst->get_sink()) {
			composit->set_sink();
		}
		// mark connections as deleted. Check for outside connections, add them.
		for (auto e : inst->get_in_edges()) {
			if (e->is_deleted()) {
				continue;
			}
			IR::Actor_Instance_Base* src_inst = e->get_source();
			if ((dynamic_cast<IR::Actor_Instance*>(src_inst) == nullptr) ||
				(dynamic_cast<IR::Actor_Instance*>(src_inst)->get_composit_actor() != composit))
			{
				std::string portname = e->get_dst_port();
				/* Edge must be adjusted */
				Connector con;
				con.first = e->get_dst_id();
				con.second = portname;
				if (used_ports.contains(portname)) {
					unsigned count = 0;
					std::string q = con.second + "_";
					while (used_ports.contains(q + std::to_string(count))) {
						count++;
					}
					portname = q + std::to_string(count);
				}

				merge_data->new_ports[con] = portname;
				used_ports.insert(portname);


				IR::Edge* n = new IR::Edge{ src_inst, composit, composit_name, portname, e->get_src_id(), e->get_src_port()};
				auto q = dpn->add_edge(n);
				composit->add_in_edge(q);
				if (e->get_feedback()) {
					n->set_feedback();
				}
				n->set_specified_size(e->get_specified_size());

				if (dynamic_cast<IR::Actor_Instance*>(src_inst) != nullptr) {
					dynamic_cast<IR::Actor_Instance*>(src_inst)->add_out_edge(q);
				}
				else {
					dynamic_cast<IR::Composit_Actor*>(src_inst)->add_out_edge(q);
				}
			}
			e->set_deleted();
		}
		for (auto e : inst->get_out_edges()) {
			if (e->is_deleted()) {
				continue;
			}
			IR::Actor_Instance_Base* sink_inst = e->get_sink();
			if ((dynamic_cast<IR::Actor_Instance*>(sink_inst) == nullptr) ||
				(dynamic_cast<IR::Actor_Instance*>(sink_inst)->get_composit_actor() != composit))
			{
				outputs.insert(inst);
				std::string portname = e->get_src_port();
				/* Edge must be adjusted */
				Connector con;
				con.first = e->get_src_id();
				con.second = portname;
				if (used_ports.contains(portname)) {
					unsigned count = 0;
					std::string q = con.second + "_";
					while (used_ports.contains(q + std::to_string(count))) {
						count++;
					}
					portname = q + std::to_string(count);
				}
				merge_data->new_ports[con] = portname;
				used_ports.insert(portname);
				IR::Edge* n = new IR::Edge{ composit, sink_inst , e->get_dst_id(), e->get_dst_port(), composit_name, portname };
				auto q = dpn->add_edge(n);
				composit->add_out_edge(q);
				if (e->get_feedback()) {
					n->set_feedback();
				}
				n->set_specified_size(e->get_specified_size());
				if (dynamic_cast<IR::Actor_Instance*>(sink_inst) != nullptr) {
					dynamic_cast<IR::Actor_Instance*>(sink_inst)->add_in_edge(q);
				}
				else {
					dynamic_cast<IR::Composit_Actor*>(sink_inst)->add_in_edge(q);
				}
			}
			e->set_deleted();
		}
		if (inst->get_out_edges().empty()) {
			outputs.insert(inst);
		}
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "New Ports:";
	for (auto x : merge_data->new_ports) {
		std::cout << " (" << x.first.first << "-" << x.first.second << " -> " << x.second << ")";
	}
	std::cout << std::endl;
#endif

	determine_edge_relations(cluster, dpn, merge_data);
	determine_channel_sizes(merge_data, cluster);

#ifdef USE_MUSTPRODUCE
	determine_must_produce(cluster, outputs, dpn, merge_data);
#endif

	determine_loopcount(merge_data, cluster);
	determine_max_loopcount(cluster, merge_data);
	determine_guaranteed_production(cluster, merge_data);
	determine_required_freechecks(cluster, merge_data);

	for (auto inst = cluster.rbegin(); inst != cluster.rend(); ++inst) {
		find_combine_actor(*inst, merge_data, merge_data->combine_actors[*inst]);
		merge_data->scm[*inst] = merge_data->sched_cond_fusion[*inst];
	}

	create_channel_vars(composit, merge_data);

	unsigned loopsz = 0;
	for (auto inst = cluster.begin(); inst != cluster.end(); ++inst) {
		unsigned l = (*inst)->get_sched_loop_bound() / merge_data->loopcount[*inst];
		if (l > loopsz) {
			loopsz = l;
		}
	}
	if (loopsz == 0) {
		loopsz = 128; // No intention behind this number, picked by chance
	}
	composit->set_sched_loop_bound(loopsz);

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Composite loop bound: " << composit->get_sched_loop_bound() << std::endl;
#endif

	/* First generate */
	for (auto inst : cluster) {
#ifdef DEBUG_OPTIMIZATION_MERGE
		//AST::print_ast(inst->get_actor()->get_ast());
#endif
		inst->copy_ast();
		AST::AST_Root* ast = inst->get_ast();
		AST_Transform::inputpattern_channelreadstmt(ast, merge_data->used_symbols);
		AST_Transform::outputexpr_channelwritestmt(ast, merge_data->used_symbols);

		add_imports(inst, composit, merge_data->added_imports);
		std::map<std::string, std::string> replacements;
		add_parameters_variables_functions(dpn, inst, composit, merge_data->used_symbols, replacements);
		merge_data->replacements[inst] = replacements;

#ifdef DEBUG_OPTIMIZATION_MERGE
		std::cout << "Replacements of " << inst->get_name() << ":\n";
		for (auto x : replacements) {
			std::cout << x.first << " --> " << x.second << std::endl;
		}
		if (replacements.empty()) {
			std::cout << "None" << std::endl;
		}
#endif

		if (!replacements.empty()) {
			for (auto action : ast->actor->actions) {
				AST_Transform::replace_identifiers_vardef(action->vars, replacements);
				AST_Transform::replace_identifiers_stmt(action->statements, replacements);
				for (auto e : action->guards) {
					AST_Transform::replace_identifiers_expr(e, replacements);
				}
			}
		}

		for (auto c : inst->get_actor()->get_const_map()) {
			if (replacements.contains(c.first)) {
				composit->get_const_map()[replacements[c.first]] = c.second;
			}
			else {
				composit->get_const_map()[c.first] = c.second;
			}
		}

#ifdef DEBUG_OPTIMIZATION_MERGE
		//AST::print_ast(ast);
#endif
	}

	merge_data->used_symbols_do.insert(merge_data->used_symbols.begin(), merge_data->used_symbols.end());
	merge_data->used_symbols_done.insert(merge_data->used_symbols.begin(), merge_data->used_symbols.end());
	merge_data->used_symbols_init.insert(merge_data->used_symbols.begin(), merge_data->used_symbols.end());
	merge_data->used_symbols_initialize.insert(merge_data->used_symbols.begin(), merge_data->used_symbols.end());

	for (auto inst : cluster) {
		AST::AST_Root* ast = inst->get_ast();
		if (ast->actor->init != nullptr) {
			add_initialize_action(inst, composit, merge_data, ast->actor->init, merge_data->used_symbols_initialize,
				merge_data->replacements[inst]);
		}
	}
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "initialization actions added.\n";
#endif

	for (auto inst = cluster.rbegin(); inst != cluster.rend(); ++inst) {
		add_to_merge(merge_data, dpn, *inst, composit, merge_data->replacements[*inst]);
	}

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "All actors added to merge." << std::endl;
#endif

	for (auto inst : cluster) {
		auto scm = merge_data->sched_cond_fusion[inst];
		if (scm->added) {
			continue;
		}
		auto d1 = composit->get_do_action();
		std::vector<AST::Statement*> stmts;
		get_do_code(scm, stmts);
		for (auto s : stmts) {
			d1->statements.push_back(s);
		}
		if (!scm->statements_done.empty()) {
			auto d2 = composit->get_done_action();
			for (auto s : scm->statements_done) {
				d2->statements.push_back(s);
			}
		}
		if (!scm->statements_init.empty()) {
			auto d2 = composit->get_init_action();
			for (auto s : scm->statements_init) {
				d2->statements.push_back(s);
			}
		}
		scm->added = true;
	}

	delete merge_data;
}

/* Start high because we gone use this as actor instance ID too, this shall avoid conflicts with existing IDs */
static unsigned cluster_counter = 500000;

static void find_clusters(
	IR::Dataflow_Network *dpn,
	unsigned core,
	std::vector<std::vector<IR::Actor_Instance*>>& clusters)
{
	std::set<IR::Actor_Instance_Base*> all;
	for (auto inst : dpn->get_actor_instances()) {
		if (inst->is_deleted()) {
			continue;
		}
		if (inst->get_mapping() == core) {
			all.insert(inst);
		}
	}

	std::vector<std::vector<IR::Actor_Instance_Base*>> components;
	Scheduling::determine_connected_clusters(all, components);

	for (auto sorted_cluster : components) {
		unsigned id = cluster_counter++;
		for (auto x : sorted_cluster) {
			dynamic_cast<IR::Actor_Instance*>(x)->set_cluster_id(id);
		}

#if CHECK_TOPOLOGY_SORT
		/* little check .... */
		for (auto x : sorted_cluster) {
			bool found = false;
			auto n = dynamic_cast<IR::Actor_Instance*>(x);
			for (auto y : sorted_cluster) {
				auto m = dynamic_cast<IR::Actor_Instance*>(y);
				if (x == y) {
					found = true;
					continue;
				}
				if (!found) {
					if (m->is_predecessor(n)) {
						std::cout << "ERROR: not sorted." << std::endl;
					}
				}
				else {
					if (n->is_predecessor(m)) {
						std::cout << "ERROR: not sorted." << std::endl;
						exit(1);
					}
				}
			}
		}
#endif
		std::vector<IR::Actor_Instance*> result;
		for (auto l : sorted_cluster) {
			result.push_back(dynamic_cast<IR::Actor_Instance*>(l));
		}
		clusters.push_back(result);
	}
}

void Merge_Optimization::core_merge(
	IR::Dataflow_Network* dpn)
{
	Config* c = Config::getInstance();

#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Start of Optimization" << std::endl;
#endif

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		std::vector<std::vector<IR::Actor_Instance*>> clusters;
		find_clusters(dpn, core, clusters);

		for (auto cluster : clusters) {
#if 0
			cluster.erase(cluster.begin());
			cluster.erase(std::find(cluster.begin(), cluster.end(), cluster.back()));
#endif
#ifdef DEBUG_OPTIMIZATION_MERGE
			std::cout << "Cluster on core " << core << ":";
			for (auto c : cluster) {
				std::cout << " " << c->get_name();
			}
			std::cout << std::endl;
#endif
			perform_merge(core, cluster, dpn);
		}
	}
}

static void read_clusters(
	std::vector<std::vector<IR::Actor_Instance*>>& clusters,
	IR::Dataflow_Network* dpn)
{
	xml_document<char>* doc = new xml_document<char>;
	Config* c = Config::getInstance();

	{
		std::ifstream config_file(c->get_merge_config_file(), std::ifstream::in);
		if (config_file.fail()) {
			throw Converter_Exception{ "Cannot open the file " + c->get_merge_config_file() };
		}
		std::stringstream merge_config_buffer;
		merge_config_buffer << config_file.rdbuf();
		std::string str_to_parse = merge_config_buffer.str();
		char* buffer = new char[str_to_parse.size() + 1];
		std::size_t length = str_to_parse.copy(buffer, str_to_parse.size() + 1);
		buffer[length] = '\0';
		doc->parse<0>(buffer);
	}

	if (strcmp(doc->first_node()->name(), "Merge") != 0) {
		// something is wrong here, root node should be merge ... bail out
		throw Converter_Exception{ "Content of Merge Config file erroneous.\n" };
	}

	unsigned config_cluster_counter = 0;
	for (const rapidxml::xml_node<>* sub_node = doc->first_node()->first_node();
		sub_node; sub_node = sub_node->next_sibling())
	{
		if (strcmp(sub_node->name(), "Cluster") == 0) {
			std::vector<std::string> cluster;
			for (auto sub_sub_node = sub_node->first_node();
				sub_sub_node; sub_sub_node = sub_sub_node->next_sibling())
			{
				if (strcmp(sub_sub_node->name(), "Node") == 0) {
					for (auto attributes = sub_sub_node->first_attribute();
						attributes; attributes = attributes->next_attribute())
					{
						if (strcmp(attributes->name(), "name") == 0) {
							cluster.push_back(attributes->value());
						}
						else {
							throw Converter_Exception{ "Content of Merge Config file erroneous.\n" };
						}
					}
				}
			}
			std::set<IR::Actor_Instance_Base*> x;
			unsigned prev_core_id = 99999;
			for (auto c : cluster) {
				IR::Actor_Instance* inst = dpn->get_actor_instance(c);
				if (inst == nullptr) {
					throw Converter_Exception{ "Content of Merge Config file erroneous, cannot find actor instance " + c + ".\n" };
				}
				x.insert(inst);
				if (prev_core_id == 99999) {
					prev_core_id = inst->get_mapping();
				}
				else if (prev_core_id != inst->get_mapping()) {
					throw Converter_Exception{ "Content of Merge Config file erroneous, nodes not mapped to same core.\n" };
				}
			}

			if (!Scheduling::is_connected(x)) {
				std::string msg = "Content of Merge Config file erroneous, cluster is not connected:";
				for (auto i : x) {
					msg += " " + i->get_name();
				}
				throw Converter_Exception{ msg + "\n" };
			}

			std::vector<IR::Actor_Instance_Base*> sorted_cluster;
			Scheduling::topology_sort_inst(x, sorted_cluster);
			std::vector<IR::Actor_Instance*> result;
			for (auto l : sorted_cluster) {
				result.push_back(dynamic_cast<IR::Actor_Instance*>(l));
			}

			unsigned id = config_cluster_counter++;
			for (auto inst : result) {
				inst->set_cluster_id(id);
			}

			clusters.push_back(result);
		}
		else {
			throw Converter_Exception{ "Content of Merge Config file erroneous.\n" };
		}
	}
}

void Merge_Optimization::config_merge(
	IR::Dataflow_Network* dpn)
{
#ifdef DEBUG_OPTIMIZATION_MERGE
	std::cout << "Start of Optimization" << std::endl;
#endif

	std::vector<std::vector<IR::Actor_Instance*>> clusters;
	read_clusters(clusters, dpn);

	for (auto cluster : clusters) {
		if (cluster.empty()) {
			continue;
		}
		unsigned core = cluster.front()->get_mapping();
#ifdef DEBUG_OPTIMIZATION_MERGE
		std::cout << "Cluster on core " << core << ":";
		for (auto inst : cluster) {
			std::cout << " " << inst->get_name();
		}
		std::cout << std::endl;
#endif
		perform_merge(core, cluster, dpn);
	}
}