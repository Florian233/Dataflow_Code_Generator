#pragma once

#include <vector>
#include <string>
#include <tuple>
#include "Tokenizer/Action_Buffer.hpp"

namespace IR {

	/* Information about the buffer access of actors and actions.
	 * Used also by actor, hence, some elements might not be used by the action class.
	 */
	typedef struct {
		std::string buffer_name;
		std::string var_name;
		unsigned tokenrate;
		// This is the same than the order in the vector, just to be sure...
		unsigned buffer_index;
		std::string type;
	} Buffer_Access;

	/* Information about and action, e.g. accessed buffers, name and guard.
	 * Information can only be accessed through getters/setters.
	 */
	class Action {
		std::string name;
		std::string guard;
		std::vector< Buffer_Access > in_buffers;
		std::vector< Buffer_Access > out_buffers;

		Action_Buffer* tokens;

		bool init_action{ false };

		bool deleted{ false };

		// Name of the function that is generated for this action, it might deviate slightly from
		// the name of action as it has to be unique and adhere to C++ conventions
		std::string function_name;

	public:
		Action(std::string name, Action_Buffer* b) : name{ name }, tokens{ b } {

		}

		Action(std::string name, Action_Buffer* b, bool init) : name{ name }, tokens{ b } {
			init_action = init;
		}

		std::string get_name(void) {
			return name;
		}

		std::string get_guard(void) {
			return guard;
		}

		void set_guard(std::string g) {
			guard = g;
		}

		void add_in_buffer(Buffer_Access b) {
			in_buffers.push_back(b);
		}

		void add_out_buffer(Buffer_Access b) {
			out_buffers.push_back(b);
		}

		std::vector< Buffer_Access >& get_in_buffers(void) {
			return in_buffers;
		}

		std::vector< Buffer_Access >& get_out_buffers(void) {
			return out_buffers;
		}

		bool is_init(void) {
			return init_action;
		}

		Action_Buffer* get_action_buffer(void) {
			return tokens;
		}

		void set_deleted(void) {
			deleted = true;
		}

		bool is_deleted(void) {
			return deleted;
		}

		void set_function_name(std::string name) {
			function_name = name;
		}

		std::string get_function_name(void) {
			return function_name;
		}
	};
}