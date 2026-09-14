#include "Actor.hpp"
#include <string>
#include <algorithm>
#include "Config/config.h"
#include "Conversion/Conversion.hpp"
#include "Reader/Reader.hpp"
#include <set>
#include "Lexer/Lexer.hpp"
#include "Parser/Parser.hpp"
#include <iostream>
#include "AST_Analysis/AST_Analysis.hpp"
#include "common/include/String_Helper.h"
#include "IR/AST/AST_Printer.hpp"


void IR::Actor::parse_buffer_access(void) {
	for (auto input : ast->actor->inports) {
		in_buffers.push_back(Buffer_Access{ input->name.name, 0, input->type.type.name, nullptr, nullptr });
	}
	for (auto outport : ast->actor->outports) {
		out_buffers.push_back(Buffer_Access{ outport->name.name, 0, outport->type.type.name, nullptr, nullptr });
	}
}

void IR::Actor::parse_schedule_fsm(void) {

	if (ast->actor->fsm == nullptr) {
		return;
	}

	initial_state = ast->actor->fsm->inital_state.name;

	for (auto fit = ast->actor->fsm->transitions.begin(); fit != ast->actor->fsm->transitions.end(); ++fit) {

		std::string state{ (*fit)->state.name };
		std::string next_state{ (*fit)->next.name };

		std::vector<std::string> vector_action_to_fire;
		for (auto t = (*fit)->actual_actions.begin(); t != (*fit)->actual_actions.end(); ++t) {
			std::string n = (*t).second;
			replace_all_substrings(n, ".", "_");
			fsm.push_back(FSM_Entry{ state, n, next_state });
		}
	}
}

static void find_actions(
	std::string prioentry,
	AST::Actor* actor,
	std::vector<std::string>& result)
{
	for (auto a : actor->actions) {
		if (a->name.name.empty()) {
			continue;
		}
		if (a->name.name == prioentry) {
			std::string n = a->name.name;
			replace_all_substrings(n, ".", "_");
			result.push_back(n);
		}
		else if ((a->name.name.size() > prioentry.size()) && (a->name.name.substr(0, prioentry.size()) == prioentry)) {
			std::string n = a->name.name;
			replace_all_substrings(n, ".", "_");
			result.push_back(n);
		}
	}
}

void IR::Actor::parse_priorities(void) {

	for (auto it = ast->actor->prios.begin(); it != ast->actor->prios.end(); ++it) {
		std::vector<std::string> processed;
		for (auto p = (*it)->prio_rel.begin(); p != (*it)->prio_rel.end(); ++p) {
			std::vector<std::string> coresp_actions;
			find_actions(p->name, ast->actor, coresp_actions);
			for (auto a = coresp_actions.begin(); a != coresp_actions.end(); ++a) {
				for (auto prev = processed.begin(); prev != processed.end(); ++prev) {
					priorities.push_back(Priority_Entry{ *prev, *a });
				}
			}
			processed.insert(processed.end(), coresp_actions.begin(), coresp_actions.end());
		}
	}

	bool changed = true;
	while (changed) {
		changed = false;
		const size_t count = priorities.size();
		for (size_t i = 0; i < count; ++i) {
			for (size_t j = 0; j < count; ++j) {
				if (priorities[i].action_low != priorities[j].action_high) {
					continue;
				}
				const std::string high = priorities[i].action_high;
				const std::string low = priorities[j].action_low;
				if (high == low) {
					continue;
				}
				bool exists = false;
				for (const auto e : priorities) {
					if (e.action_high == high && e.action_low == low) {
						exists = true;
						break;
					}
				}
				if (!exists) {
					priorities.push_back(Priority_Entry{ high, low });
					changed = true;
				}
			}
		}
	}
}

void IR::Actor::parse_action(AST::Action *action, std::map<std::string, std::string>& symbol_map) {

	IR::Action* ir_action = new IR::Action{ action->name.name, action };
	actions.push_back(ir_action);

	std::vector<std::string> found_buffers;
	for (auto input : action->input_patterns) {
		found_buffers.push_back(input->port.name);
		unsigned count = static_cast<unsigned>(input->IDs.size());
		if (input->repeat != nullptr) {
			count *= Conversion_Helper::evaluate_constant_expression(input->repeat, symbol_map);
		}
		ir_action->add_in_buffer(Buffer_Access{ input->port.name, count, get_in_port_type(input->port.name), input, nullptr });
	}

	// Now add all buffers that were not used by this action with tokenrate zero
	for (auto it = in_buffers.begin(); it != in_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			ir_action->add_in_buffer(Buffer_Access{ it->buffer_name, 0, get_in_port_type(it->buffer_name), nullptr, nullptr});
		}
	}

	found_buffers.clear();
	for (auto output : action->output_expressions) {
		found_buffers.push_back(output->port.name);
		unsigned count = static_cast<unsigned>(output->expressions.size());
		if (output->repeat != nullptr) {
			count *= Conversion_Helper::evaluate_constant_expression(output->repeat, symbol_map);
		}
		ir_action->add_out_buffer(Buffer_Access{ output->port.name, count, get_out_port_type(output->port.name), nullptr, output });
	}

	//Now add all buffers that are not used by this action with tokenrate zero
	for (auto it = out_buffers.begin(); it != out_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			ir_action->add_out_buffer(Buffer_Access{ it->buffer_name, 0, get_out_port_type(it->buffer_name), nullptr, nullptr});
		}
	}
}

void IR::Actor::convert_import(AST::Import *import, Dataflow_Network* dpn) {

	std::string path = import->path.name;
	replace_all_substrings(path, ".", "\\");

	Unit* u = dpn->get_unit(path);

	if (u == nullptr) {
		std::filesystem::path p{ path };
		u = Network_Reader::read_unit(dpn, p);
		u->initialize(dpn);
		dpn->add_unit(path, u);
	}
	this->imported_symbols.push_back(u);
}

void IR::Actor::transform_IR(Dataflow_Network* dpn) {
	Config* c = Config::getInstance();
	if (c->get_verbose_ir_gen()) {
		std::cout << "IR transformation of " << class_name << "." << std::endl;
	}

	Lexer::Lexer l{code};
	Parser::Parser_Class p{ &l };
	try {
		ast = p.parse();
	}
	catch (const Parser::Parser_Exception_Unexpected_Symbol &e) {
		std::cerr << "Error while parsing: " << this->path << std::endl;
		std::cerr << e.what();
		exit(1);
	}

	for (auto i : ast->imports) {
		convert_import(i, dpn);
	}
	std::vector<AST::Unit*> units;
	for (auto u : imported_symbols) {
		units.push_back(u->get_ast()->unit);
		for (auto uu : u->get_imported_units()) {
			units.push_back(uu->unit);
		}
	}

	AST_Analysis::semantic(units, ast);

	actor_name = ast->actor->name.name;
	for (auto v = ast->actor->vars.begin(); v != ast->actor->vars.end(); ++v) {
		Conversion_Helper::read_constants(*v, const_map);
	}
	for (auto x : units) {
		for (auto v : x->vars) {
			Conversion_Helper::read_constants(v, const_map);
		}
	}

	/* Set name for every anonymous action to enable name based lookups. */
	{
		std::set<std::string> used_action_names;
		for (auto a : ast->actor->actions) {
			if (!a->name.name.empty()) {
				used_action_names.insert(a->name.name);
			}
		}
		unsigned anonymous_action_counter = 0;
		for (auto a : ast->actor->actions) {
			if (!a->name.name.empty()) {
				continue;
			}
			std::string candidate;
			do {
				candidate = "action_" + std::to_string(anonymous_action_counter++);
			} while (used_action_names.count(candidate) != 0);
			a->name.name = candidate;
			used_action_names.insert(candidate);
		}
	}

	parse_buffer_access();
	parse_schedule_fsm();


	// Now determine the token rates
	for (auto it = ast->actor->actions.begin(); it != ast->actor->actions.end(); ++it) {
		parse_action(*it, const_map);
	}
	if (ast->actor->init != nullptr) {
		actions.push_back(new IR::Action{ "initialize", ast->actor->init, true});
	}

	parse_priorities();

	/* Must be done after prios and fsm are parsed. */
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		std::string n = (*it)->get_name();
		replace_all_substrings(n, ".", "_");
		(*it)->set_name(n);
	}

	if (c->get_verbose_ir_gen()) {
		std::cout << "Number of actions: " << ast->actor->actions.size() << std::endl;
		for (auto it = in_buffers.begin(); it != in_buffers.end(); ++it) {
			std::cout << "Input Port: " << it->buffer_name << ", type: " << it->type << std::endl;
		}
		for (auto it = out_buffers.begin(); it != out_buffers.end(); ++it) {
			std::cout << "Output Port: " << it->buffer_name << ", type: " << it->type << std::endl;
		}
		for (auto it = priorities.begin(); it != priorities.end(); ++it) {
			std::cout << "Priority: " << it->action_high << " > " << it->action_low << ";" << std::endl;
		}
		if (!initial_state.empty()) {
			std::cout << "Initial state: " << initial_state << std::endl;
		}
		for (auto it = fsm.begin(); it != fsm.end(); ++it) {
			std::cout << "FSM: " << it->state << "(" << it->action << ") -> " << it->next_state << ";" << std::endl;
		}
		for (auto it = actions.begin(); it != actions.end(); ++it) {
			std::cout << "Found action: " << (*it)->get_name() << std::endl;
			for (auto i_it = (*it)->get_in_buffers().begin();
				i_it != (*it)->get_in_buffers().end();
				++i_it)
			{
				std::cout << "Input buffer: " << i_it->buffer_name << ", tokenrate: " << i_it->tokenrate << std::endl;
			}
			for (auto o_it = (*it)->get_out_buffers().begin();
				o_it != (*it)->get_out_buffers().end();
				++o_it)
			{
				std::cout << "Output buffer: " << o_it->buffer_name << ", tokenrate: "
					<< o_it->tokenrate << std::endl;
			}
		}

		for (auto it = const_map.begin(); it != const_map.end(); ++it) {
			std::cout << "Symbol: " << it->first << " Value: " << it->second << std::endl;
		}

		std::cout << "IR transformation of " << class_name << " done." << std::endl;
	}
}

void IR::Unit::convert_imports(IR::Dataflow_Network* dpn) {
	for (auto it = ast->imports.begin(); it != ast->imports.end(); ++it) {

		std::string path = (*it)->path.name;
		replace_all_substrings(path, ".", "\\");

		Unit* u = dpn->get_unit(path);

		if (u == nullptr) {
			std::filesystem::path p{ path };
			u = Network_Reader::read_unit(dpn, p);
			sub_units.push_back(u);
			u->initialize(dpn);
			dpn->add_unit(path, u);
		}

		sub_units.insert(sub_units.end(), u->get_sub_units().begin(), u->get_sub_units().end());
	}
}