#pragma once

#include <string>
#include "AST/AST.hpp"
#include "Actor_Instance_Base.hpp"
#include "Dataflow_Analysis/Actor_Classification/Actor_Classification.hpp"

namespace IR {

	/* Information about a composit actor. */
	class Composit_Actor : public Actor_Instance_Base {

		AST::AST_Root* ast;
		std::map<std::string, std::string> const_map;

		AST::Action* done_action = nullptr;
		AST::Action* do_action = nullptr;
		AST::Action* init_action = nullptr;
		AST::Action* initialize_action = nullptr;

		std::string name;
		unsigned id;
		unsigned loop_bound = 0;
		unsigned mapping = 0;
		unsigned sched_prio = 60;

		std::vector<Edge*> in_edges;
		std::vector<Edge*> out_edges;

		bool is_source = false;
		bool is_sink = false;

		std::vector<Unit*> imported_symbols;

		/* Classification of the input channel tokenrates. Might differ from the output classification! */
		Actor_Classification input_classification{ Actor_Classification::dynamic_rate };
		/* Classification of the output channel tokenrates, might differ from the input classification! */
		Actor_Classification output_classification{ Actor_Classification::dynamic_rate };

	public:
		Composit_Actor(std::string _name, unsigned _id, unsigned _core) : name{ _name }, id{ _id }, mapping{ _core } {
			ast = new AST::AST_Root{};
			ast->actor = new AST::Actor{};
			AST::Action_Priority* prio = new AST::Action_Priority{};
			prio->prio_rel.push_back(AST::ID{ .name = "init_action" });
			prio->prio_rel.push_back(AST::ID{ .name = "do_action" });
			prio->prio_rel.push_back(AST::ID{ .name = "done_action" });
		};

		AST::AST_Root* get_ast(void) {
			return ast;
		}

		std::string get_class(void) {
			return name;
		}

		std::string get_name(void) {
			return name + "_inst";
		}

		std::map<std::string, std::string>& get_const_map(void) {
			return const_map;
		}

		AST::Action* get_done_action(void)
		{
			if (done_action == nullptr) {
				done_action = new AST::Action{};
				done_action->name.name = "done_action";
				ast->actor->actions.push_back(done_action);
			}
			return done_action;
		}
		AST::Action* get_do_action(void)
		{
			if (do_action == nullptr) {
				do_action = new AST::Action{};
				do_action->name.name = "do_action";
				ast->actor->actions.push_back(do_action);
			}
			return do_action;
		}
		AST::Action* get_init_action(void)
		{
			if (init_action == nullptr) {
				init_action = new AST::Action{};
				init_action->name.name = "init_action";
				ast->actor->actions.push_back(init_action);
			}
			return init_action;
		}
		AST::Action* get_initialize_action(void)
		{
			if (initialize_action == nullptr) {
				initialize_action = new AST::Action{};
				ast->actor->init = initialize_action;
			}
			return initialize_action;
		}

		unsigned get_sched_loop_bound(void) {
			return loop_bound;
		}
		void set_sched_loop_bound(unsigned b) {
			loop_bound = b;
		}

		unsigned get_mapping(void) {
			return mapping;
		}

		std::vector<IR::Edge*>& get_out_edges(void) {
			return out_edges;
		}

		std::vector<IR::Edge*>& get_in_edges(void) {
			return in_edges;
		}

		void add_in_edge(IR::Edge* e) {
			in_edges.push_back(e);
		}

		void add_out_edge(IR::Edge* e) {
			out_edges.push_back(e);
		}

		std::vector<Unit*>& get_imported_symbols(void) {
			return imported_symbols;
		}
		void add_imported_symbol(Unit* u) {
			imported_symbols.push_back(u);
		}

		Actor_Classification get_input_classification(void) {
			return input_classification;
		}
		void set_input_classification(Actor_Classification a) {
			input_classification = a;
		}
		Actor_Classification get_output_classification(void) {
			return output_classification;
		}
		void set_output_classification(Actor_Classification a) {
			output_classification = a;
		}

		unsigned get_sched_prio(void) {
			return sched_prio;
		}

		void set_sched_prio(unsigned prio) {
			sched_prio = prio;
		}

		void set_source(void) {
			is_source = true;
		}

		bool get_source(void) const {
			return is_source;
		}

		void set_sink(void) {
			is_sink = true;
		}

		bool get_sink(void) const {
			return is_sink;
		}
	};
}