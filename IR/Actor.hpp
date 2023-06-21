#pragma once

#include <string>
#include "Action.hpp"
#include <vector>
#include "Tokenizer/Action_Buffer.hpp"
#include "Tokenizer/Import_Buffer.hpp"
#include "Tokenizer/Method_Buffer.hpp"
#include "Tokenizer/Parameter_Buffer.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include "Tokenizer/Var_Buffer.hpp"
#include "Tokenizer/Native_Buffer.hpp"
#include "Conversion/Actor_Conversion_Data.hpp"
#include "Actor_Classification/Actor_Classification.hpp"

namespace IR {

	/* FSM entry state(action) -> next_state
	 * If multiple actions can cause this state transition create multiple objects!
	 */
	typedef struct {
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
		void convert_import(Import_Buffer& token_producer);

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

		void read_actor(void);

		void transform_IR(void);

		void classify_actor(void);

		std::vector<Buffer_Access>& get_in_buffers(void) {
			return in_buffers;
		}

		std::vector<Buffer_Access>& get_out_buffers(void) {
			return out_buffers;
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
	};
}