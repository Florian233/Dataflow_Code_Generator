#pragma once

#include <vector>
#include <string>
#include <tuple>
#include "AST/AST.hpp"

namespace IR {

	/* Information about the buffer access of actors and actions.
	 * Used also by actor, hence, some elements might not be used by the action class.
	 */
	typedef struct ba {
		std::string buffer_name;
		unsigned tokenrate;
		std::string type;
		AST::Input_Pattern* in;
		AST::Output_Expression* out;
	} Buffer_Access;

	/* Information about and action, e.g. accessed buffers, name and guard.
	 * Information can only be accessed through getters/setters.
	 */
	class Action {
		std::string name;
		std::vector< Buffer_Access > in_buffers;
		std::vector< Buffer_Access > out_buffers;

		AST::Action* tokens;

		bool init_action{ false };

		bool deleted{ false };

		// Name of the function that is generated for this action, it might deviate slightly from
		// the name of action as it has to be unique and adhere to C++ conventions
		std::string function_name;

	public:
		Action(std::string name, AST::Action* b) : name{ name }, tokens{ b } {

		}
#if 0
		Action(std::string name) : name{ name } {
			tokens = nullptr;
		}
#endif

		Action(std::string name, AST::Action* b, bool init) : name{ name }, tokens{ b } {
			init_action = init;
		}

		std::string get_name(void) {
			return name;
		}
		void set_name(std::string n) {
			name = n;
		}

		bool has_guard(void) {
			if  (tokens == nullptr) {
				return false;
			}
			return !tokens->guards.empty();
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

		AST::Action* get_ast(void) {
			return tokens;
		}
	};
}