#pragma once

#include <string>
#include "Action.hpp"
#include "Unit.hpp"
#include <vector>
#include <set>
#include "Tokenizer/Action_Buffer.hpp"
#include "Tokenizer/Import_Buffer.hpp"
#include "Tokenizer/Method_Buffer.hpp"
#include "Tokenizer/Parameter_Buffer.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include "Tokenizer/Var_Buffer.hpp"
#include "Tokenizer/Native_Buffer.hpp"
#include "Conversion/Actor_Conversion_Data.hpp"
#include "Dataflow_Analysis/Actor_Classification/Actor_Classification.hpp"

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
	 * No transitivity considered!
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
		/* Also add transitive closure! This is required to simplify sorting. */
		std::vector<Priority_Entry> priorities;

		std::vector< Buffer_Access > in_buffers;
		std::vector< Buffer_Access > out_buffers;

		std::vector<Action_Buffer> buffered_actions{};
		std::vector<Import_Buffer> import_buffers{};
		std::vector<Var_Buffer> var_buffers{};
		std::vector<Parameter_Buffer> param_buffers{};
		std::vector<Method_Buffer> method_buffers{};
		std::vector<Native_Buffer> native_buffers{};

		std::vector<std::pair<std::string, Unit*>> imported_symbols;

		/* uses native code */
		bool use_native{ false };

		/* Translate the buffer access definitions of this actor into objects stored here as
		 * in_buffers and out_buffers.
		 */
		void parse_buffer_access(Token& t, Tokenizer& token_producer);
		/* Convert the FSM definition into a FSM_Entry objects stored in this object. */
		void parse_schedule_fsm(Token& t, Tokenizer& token_producer);
		/* Convert the Priority definitions into Priority_Entry objects stored in this object. */
		void parse_priorities(Token& t, Tokenizer& token_producer);
		/* Parse the actions for their token rates and guards. */
		void parse_action(Action_Buffer& token_producer);
		/* Convert the imports as there might be consts defined that are used for the token rates. */
		void convert_import(Import_Buffer& token_producer, Dataflow_Network* dpn);

		/* The data that is used during and for conversion that is not relevant for the IR. */
		Actor_Conversion_Data conversion_data;

		/* Classification of the input channel tokenrates. Might differ from the output classification! */
		Actor_Classification input_classification{ Actor_Classification::dynamic_rate };
		/* Classification of the output channel tokenrates, might differ from the input classification! */
		Actor_Classification output_classification{ Actor_Classification::dynamic_rate };

	public:

		Actor(std::string name, std::string path) : class_name(name), path(path) {

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

		Actor_Conversion_Data& get_conversion_data(void) {
			return conversion_data;
		}

		Actor_Conversion_Data* get_conversion_data_ptr(void) {
			return &conversion_data;
		}

		std::vector<Action_Buffer>& get_buffered_actions(void) {
			return buffered_actions;
		}

		std::vector<Var_Buffer>& get_var_buffers(void) {
			return var_buffers;
		}

		std::vector<Parameter_Buffer>& get_param_buffers(void) {
			return param_buffers;
		}
			
		std::vector<Method_Buffer>& get_method_buffers(void) {
			return method_buffers;
		}
			
		std::vector<Native_Buffer>& get_native_buffers(void) {
			return native_buffers;
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

		Actor_Classification get_input_classification(void) {
			return input_classification;
		}

		Actor_Classification get_output_classification(void) {
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
			for (auto p : priorities) {//FIXME: This is not complete! Use comparison_object from scheduling lib!
				if ((p.action_high == action1->get_name()) && (p.action_low == action2->get_name())) {
					return action1;
				}
				else if ((p.action_high == action2->get_name()) && (p.action_low == action1->get_name())) {
					return action2;
				}
			}

			return nullptr;
		}

		bool is_static(void) {
			if ((input_classification == Actor_Classification::static_rate) &&
				(output_classification == Actor_Classification::static_rate))
			{
				return true;
			}
			return false;
		}

		std::vector<Import_Buffer>& get_import_buffers(void) {
			return import_buffers;
		}

		bool uses_native(void) {
			return use_native;
		}

		std::vector<std::pair<std::string, Unit*>>& get_imported_symbols(void) {
			return imported_symbols;
		}
	};
}