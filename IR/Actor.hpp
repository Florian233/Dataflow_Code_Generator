#pragma once

#include <string>
#include "Action.hpp"
#include <vector>
#include <set>
#include "Dataflow_Analysis/Actor_Classification/Actor_Classification.hpp"
#include "AST/AST.hpp"
#include "Unit.hpp"
#include <map>

namespace IR {

	/* FSM entry state(action) -> next_state
	 * If multiple actions can cause this state transition create multiple objects!
	 */
	typedef struct FSM_entry_s{
		std::string state;
		std::string action;
		std::string next_state;
	} FSM_Entry;

	/* Priority releation between two actions action_high has higher priority than action_low.
	 */
	typedef struct {
		std::string action_high;
		std::string action_low;
	} Priority_Entry;

	/* Information about an actor, serves also as container for all token buffers that are processed during the
	 * whole code conversion.
	 */
	class Actor {
		std::string code;
		std::string path;
		std::string class_name; // This name is given by the network file and file name
		std::string actor_name; // This name is used in the actor definition
		std::vector<Action*> actions;

		/* Multiple entries for the same state transition if it can be caused by multiple actions! */
		std::vector<FSM_Entry> fsm;
		std::string initial_state;
		/* Transitive closure is added by parse_priorities(). This is required to simplify sorting. */
		std::vector<Priority_Entry> priorities;

		std::vector< Buffer_Access > in_buffers;
		std::vector< Buffer_Access > out_buffers;

		AST::AST_Root* ast;

		std::vector<Unit*> imported_symbols;

		/* Translate the buffer access definitions of this actor into objects stored here as
		 * in_buffers and out_buffers.
		 */
		void parse_buffer_access(void);
		/* Convert the FSM definition into a FSM_Entry objects stored in this object. */
		void parse_schedule_fsm(void);
		/* Convert the Priority definitions into Priority_Entry objects stored in this object. */
		void parse_priorities(void);
		/* Parse the actions for their token rates and guards. */
		void parse_action(AST::Action *action, std::map<std::string, std::string>& symbol_map);
		/* Convert the imports as there might be consts defined that are used for the token rates. */
		void convert_import(AST::Import* import, Dataflow_Network* dpn);

		/* Classification of the input channel tokenrates. Might differ from the output classification! */
		Actor_Classification input_classification{ Actor_Classification::dynamic_rate };
		/* Classification of the output channel tokenrates, might differ from the input classification! */
		Actor_Classification output_classification{ Actor_Classification::dynamic_rate };

		std::string identifier;

		std::map<std::string, std::string> const_map;

	public:

		Actor(std::string name, std::string path) : class_name(name), path(path) {
			ast = nullptr;
		}

		std::string get_class_name(void) {
			return class_name;
		}

		std::string get_path(void) {
			return path;
		}

		std::vector<Action*>& get_actions(void) {
			return actions;
		}

		Action* get_action(std::string name) {
			for (auto it = actions.begin(); it != actions.end(); ++it) {
				if ((*it)->get_name() == name) {
					return *it;
				}
			}

			return NULL;
		}

		Action* get_init_action(void) {
			for (auto it = actions.begin(); it != actions.end(); ++it) {
				if ((*it)->is_init()) {
					return *it;
				}
			}

			return NULL;
		}

		void read_actor(void);

		void transform_IR(Dataflow_Network *dpn);

		void classify_actor(void);

		std::vector<Buffer_Access>& get_in_buffers(void) {
			return in_buffers;
		}

		std::vector<Buffer_Access>& get_out_buffers(void) {
			return out_buffers;
		}

		std::string get_in_port_type(std::string p) {
			for (auto in = in_buffers.begin(); in != in_buffers.end(); ++in) {
				if (in->buffer_name == p) {
					return in->type;
				}
			}
			return "";
		}

		std::string get_out_port_type(std::string p) {
			for (auto out = out_buffers.begin(); out != out_buffers.end(); ++out) {
				if (out->buffer_name == p) {
					return out->type;
				}
			}
			return "";
		}

		std::vector<FSM_Entry>& get_fsm(void) {
			return fsm;
		}

		std::string get_initial_state(void) {
			return initial_state;
		}

		std::vector<Priority_Entry>& get_priorities(void) {
			return priorities;
		}

		Actor_Classification get_input_classification(void) const {
			return input_classification;
		}

		Actor_Classification get_output_classification(void) const {
			return output_classification;
		}

		std::vector<IR::Action*> find_schedulable_actions(
			std::string state)
		{
			std::vector<IR::Action*> output;

			if (state == "") {
				for (auto it = actions.begin(); it != actions.end(); ++it) {
					if ((*it)->is_init()) {
						continue;
					}
					output.push_back(*it);
				}
			}
			else {
				//go through all fsm entries and find the ones for the given state
				for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
					if (fsm_it->state == state) {
						//fsm_it->action might only be the tag, hence, we have to compare with the actual action names
						for (auto it = actions.begin(); it != actions.end(); ++it) {
							if ((*it)->is_init()) {
								continue;
							}
							if ((fsm_it->action == (*it)->get_name())
								|| (((*it)->get_name().find(fsm_it->action) == 0)
									&& ((*it)->get_name()[fsm_it->action.size()] == '$')))
							{
								output.push_back(*it);
							}
						}
					}
				}
			}
			return output;
		}

		/*
			Finds for a given state and the action that has been fired in this state
			the corresponding next state according to the transitions of the FSM.
		*/
		std::string find_next_state(
			std::string current_state,
			std::string fired_action)
		{
			for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
				if ((fsm_it->state == current_state)
					&& ((fsm_it->action == fired_action)
						|| ((fired_action.find(fsm_it->action) == 0) && (fired_action[fsm_it->action.size()] == '$'))))
				{
					return fsm_it->next_state;
				}

			}

			return "";
		}

		void get_all_states(std::set<std::string> &result)
		{
			for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
				result.insert(fsm_it->state);
				result.insert(fsm_it->next_state);
			}
			if (result.empty()) {
				//indicate that we don't have states, this "value" can be forwarded to other services directly,
				//the caller doesn't need to care whether this set is empty or not
				result.insert("");
			}
		}

		// returns either a pointer of higher priority action or nullptr if there is not priority between them defined
		IR::Action* get_higher_priority(IR::Action *action1, IR::Action *action2) {
			for (auto p = priorities.begin(); p != priorities.end(); ++p) {
				if ((p->action_high == action1->get_name()) && (p->action_low == action2->get_name())) {
					return action1;
				}
				else if ((p->action_high == action2->get_name()) && (p->action_low == action1->get_name())) {
					return action2;
				}
			}

			return nullptr;
		}

		bool is_static(void) const {
			if ((input_classification == Actor_Classification::static_rate) &&
				(output_classification == Actor_Classification::static_rate))
			{
				return true;
			}
			return false;
		}

		std::vector<Unit*>& get_imported_symbols(void) {
			return imported_symbols;
		}

		void set_identifier(std::string i) {
			identifier = i;
		}
		std::string get_identifier(void) {
			return identifier;
		}

		AST::AST_Root* get_ast(void) {
			return ast;
		}

		std::map<std::string, std::string>& get_const_map(void) {
			return const_map;
		}
	};
}