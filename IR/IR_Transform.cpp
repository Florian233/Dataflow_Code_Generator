#include "Actor.hpp"
#include "Config/debug.h"
#include <string>
#include <algorithm>
#include "Config/config.h"
#include "Conversion/Unit_File_Converter.hpp"
#include "Conversion/Converter_RVC_Cpp.hpp"


//TODO Maybe const values inside the actor might serve for type size specification for the
//channels. Hence, it might be necessary to convert the variables of the actor before.
//Code is in actor conversion
void IR::Actor::parse_buffer_access(Token& t, Tokenizer& token_producer) {
	std::string type;
	std::string name;
	unsigned index = 0;
	bool add{ false };

	while (t.str != "==>") {
		if (t.str == ",") {
			in_buffers.push_back(Buffer_Access{ name, "", 0, index, type });
			conversion_data.add_channel(name, type);
			add = false;
			++index;
			t = token_producer.get_next_Token();
		}
		else {
			type = Converter_RVC_Cpp::convert_type(t, token_producer, conversion_data.get_symbol_map());
			name = t.str;
			t = token_producer.get_next_Token();
			add = true;
		}
	}

	if (add) {
		in_buffers.push_back(Buffer_Access{ name, "", 0, index, type });
		conversion_data.add_channel(name, type);
		add = false;
	}
	index = 0;

	// now pointing at ==>
	t = token_producer.get_next_Token();

	while (t.str != ":") {
		if (t.str == ",") {
			out_buffers.push_back(Buffer_Access{ name, "", 0, index, type });
			conversion_data.add_channel(name, type);
			add = false;
			++index;
			t = token_producer.get_next_Token();
		}
		else {
			type = Converter_RVC_Cpp::convert_type(t, token_producer, conversion_data.get_symbol_map());
			name = t.str;
			add = true;
			t = token_producer.get_next_Token();
		}
	}
	if (add) {
		out_buffers.push_back(Buffer_Access{ name, "", 0, index, type });
		conversion_data.add_channel(name, type);
	}
}

void IR::Actor::parse_schedule_fsm(Token& t, Tokenizer& token_producer) {

	//token contains schedule 
	t = token_producer.get_next_Token(); //contains fsm
	t = token_producer.get_next_Token(); //contains starting state
	initial_state.append(t.str);
	t = token_producer.get_next_Token(); //contains :
	t = token_producer.get_next_Token(); //start of the fsm body
	while (t.str != "end") {
		std::string state{ t.str };
		t = token_producer.get_next_Token(); // ( 
		//there can be multiple actions to be fired to trigger this transition!
		std::vector<std::string> vector_action_to_fire;
		while (t.str != ")") {
			t = token_producer.get_next_Token();
			std::string action{ t.str };
			t = token_producer.get_next_Token();
			while ((t.str != ",") && (t.str != ")")) {
				if (t.str == ".") {
					action.append("$");
				}
				else {
					action.append(t.str);
				}
				t = token_producer.get_next_Token();
			}
			vector_action_to_fire.push_back(action);
		}
		t = token_producer.get_next_Token(); // -->
		t = token_producer.get_next_Token();
		std::string next_state{ t.str };
		t = token_producer.get_next_Token(); // ;
		t = token_producer.get_next_Token();
		//add all transitions to corresponding class members
		for (auto it = vector_action_to_fire.begin(); it != vector_action_to_fire.end(); ++it) {
			fsm.push_back(FSM_Entry{state, *it , next_state});
		}
	}

	//move one token past the end of the FSM block
	t = token_producer.get_next_Token();
}

void IR::Actor::parse_priorities(Token& t, Tokenizer& token_producer) {
	t = token_producer.get_next_Token();
	std::vector<std::string> greater_actions;
	while (t.str != "end") {
		if (t.str == ";") {
			t = token_producer.get_next_Token();
		}
		else if (t.str == ">") {
			t = token_producer.get_next_Token();
			std::string lesser{ t.str };
			t = token_producer.get_next_Token();
			while ((t.str != ">") && (t.str != ";")) {
				if (t.str == ".") {
					lesser.append("$");
				} else {
					lesser.append(t.str);
				}
				t = token_producer.get_next_Token();
			}
			//insert transitive closure for sorting
			for (auto greater_it = greater_actions.begin();
				greater_it != greater_actions.end(); ++greater_it)
			{
				priorities.push_back(Priority_Entry{*greater_it, lesser});
			}
			greater_actions.push_back(lesser);
		}
		else {//start of one row of priority relations
			std::string greater = t.str;
			t = token_producer.get_next_Token();
			if (t.str == ".") {
				t = token_producer.get_next_Token();
				while (t.str != ">" && t.str != ";") {
					if (t.str == ".") {
						greater.append("$");
					}
					else {
						greater.append(t.str);
					}
					t = token_producer.get_next_Token();
				}
			}
			greater_actions.clear();
			greater_actions.push_back(greater);
		}
	}
	//move one token past the end of the block
	t = token_producer.get_next_Token();
}

static unsigned anonymous_action_counter{ 0 };

static std::string skip_bracket(Token& t, Action_Buffer& token_producer) {
	std::string ret;
	if (t.str == "[") {
		ret = t.str + " ";
		t = token_producer.get_next_Token();
		while (t.str != "]") {
			if (t.str == "[") {
				ret.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				ret.append(" " + t.str);
				t = token_producer.get_next_Token();
			}
		}
		ret.append(t.str);
		t = token_producer.get_next_Token();
	}
	return ret;
}

void IR::Actor::parse_action(Action_Buffer& token_producer) {
	Token t = token_producer.get_next_Token();

	std::string name;

	if (t.str == "initialize") {
		// nothing to do here
		actions.push_back(new IR::Action{ t.str, &token_producer, true });
		token_producer.reset_buffer();
		return;
	}

	if (t.str != "action") {
		while (t.str != ":") {
			name.append(t.str);
			t = token_producer.get_next_Token();
		}
		//skip :
		t = token_producer.get_next_Token();
	}
	else {
		name = "action";
		name.append(std::to_string(anonymous_action_counter));
		++anonymous_action_counter;
	}

	IR::Action* action = new IR::Action{ name, &token_producer };
	actions.push_back(action);

	//skip action
	t = token_producer.get_next_Token();

	std::vector<std::string> found_buffers;
	unsigned index = 0;
	// must be a buffer name or ==> if no input buffer is used
	while (t.str != "==>") {
		std::string buffer_name{};
		std::string var_name{};
		unsigned count = 0;
		if (t.str != "[") {
			//Buffer name explicitly stated
			buffer_name = t.str;
			t = token_producer.get_next_Token();
			// must point to : now
			if (t.str != ":") {
				throw Wrong_Token_Exception{ "Expected \":\" but found \"" + t.str + "\"." };
			}
			t = token_producer.get_next_Token();
		}
		else {
			//buffer name according to port declarations of actor
			buffer_name = in_buffers[index].buffer_name;
			++index;
		}

		//now points at [, this can be skipped
		if (t.str != "[") {
			throw Wrong_Token_Exception{ "Expected \"[\" but found \"" + t.str + "\"." };
		}
		t = token_producer.get_next_Token();

		while (t.str != "]") {
			if (t.str == ",") {
				t = token_producer.get_next_Token();
			}
			else if (t.str == "[") {
				var_name.append(skip_bracket(t, token_producer));
				++count;
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				var_name.append(t.str);
				++count;
				t = token_producer.get_next_Token();
			}
		}

		// now points at ]
		t = token_producer.get_next_Token();
		if (t.str == "repeat") {
			t = token_producer.get_next_Token();
			std::string expression;
			while ((t.str != ",") && (t.str != "==>")) {
				expression.append(t.str);
				t = token_producer.get_next_Token();
			}
			count *= Converter_RVC_Cpp::evaluate_constant_expression(expression, conversion_data.get_symbol_map(),
																	conversion_data.get_symbol_map());
		}

		action->add_in_buffer(Buffer_Access{ buffer_name, var_name, count });
		found_buffers.push_back(buffer_name);

		//now points at , or ==>, skip it points at ,
		if (t.str == ",") {
			t = token_producer.get_next_Token();
		}
	}

	// Now add all buffers that were not used by this action with tokenrate zero
	for (auto it = in_buffers.begin(); it != in_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			action->add_in_buffer(Buffer_Access{ it->buffer_name, "", 0 });
		}
	}

	found_buffers.clear();

	index = 0;
	//now points at ==>, skip it
	t = token_producer.get_next_Token();

	while ((t.str != "guard") && (t.str != "var") && (t.str != "do")
		&& (t.str != "end") && (t.str != "endaction"))
	{
		std::string buffer_name{};
		std::string var_name{};
		unsigned count = 0;
		if (t.str != "[") {
			//Buffer name explicitly stated
			buffer_name = t.str;
			t = token_producer.get_next_Token();
			// must point to : now
			if (t.str != ":") {
				throw Wrong_Token_Exception{ "Expected \":\" but found \"" + t.str + "\"." };
			}
			t = token_producer.get_next_Token();
		}
		else {
			//buffer name according to port declarations of actor
			buffer_name = out_buffers[index].buffer_name;
			++index;
		}

		//now points at [, this can be skipped
		if (t.str != "[") {
			throw Wrong_Token_Exception{ "Expected \"[\" but found \"" + t.str + "\"." };
		}
		t = token_producer.get_next_Token();

		while (t.str != "]") {
			if (t.str == ",") {
				t = token_producer.get_next_Token();
			}
			else if (t.str == "[") {
				var_name.append(skip_bracket(t, token_producer));
				++count;
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				var_name.append(t.str);
				++count;
				t = token_producer.get_next_Token();
			}
		}

		// now points at ]
		t = token_producer.get_next_Token();
		if (t.str == "repeat") {
			t = token_producer.get_next_Token();
			std::string expression;
			while ((t.str != ",") && (t.str != "guard") && (t.str != "var")
				&& (t.str != "do") && (t.str != "end") && (t.str != "endaction"))
			{
				expression.append(t.str);
				t = token_producer.get_next_Token();
			}
			count *= Converter_RVC_Cpp::evaluate_constant_expression(expression, conversion_data.get_symbol_map(),
																	conversion_data.get_symbol_map());
		}

		action->add_out_buffer(Buffer_Access{ buffer_name, var_name, count });
		found_buffers.push_back(buffer_name);

		//now points at , or ==>, skip it points at ,
		if (t.str == ",") {
			t = token_producer.get_next_Token();
		}
	}

	//Now add all buffers that are not used by this action with tokenrate zero
	for (auto it = out_buffers.begin(); it != out_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			action->add_out_buffer(Buffer_Access{ it->buffer_name, "", 0 });
		}
	}

	while ((t.str != "guard") && (t.str != "end") && (t.str != "actionend")) {
		//skip the tokens until we find the end or the guard
		t = token_producer.get_next_Token();
	}

	if (t.str == "guard") {
		//okay, we found the guard, let's store it somewhere...
		std::string guard{};
		//skip the guard token, we don't need that
		t = token_producer.get_next_Token();
		while ((t.str != "var") && (t.str != "do") && (t.str != "end") && (t.str != "endaction")) {
			if (guard.size() == 0) {
				guard.append(t.str);
			}
			else {
				guard.append(" ");
				// The beginning can never be an comparison operation, otherwise it is wrong anyhow
				if (t.str == "=") {
					guard.append("==");
				}
				else {
					guard.append(t.str);
				}
			}
			t = token_producer.get_next_Token();
		}
		action->set_guard(guard);
	}

	// now reset the buffer as it will be used later on to translate the action to C++ code
	token_producer.reset_buffer();
}

void IR::Actor::convert_import(Import_Buffer& token_producer) {
	Config* c = c->getInstance();
	std::string path_to_import_file{ c->get_source_dir() };
	bool parsing_import = false;
	std::string previous_token;
	std::string symbol{};

	for (auto it = token_producer.begin(); it != token_producer.end(); ++it) {
		if (it->str == "import") {
			if (parsing_import == true) {
				throw Wrong_Token_Exception{ "Unexpected Token." };
			}
			path_to_import_file.clear();
			parsing_import = true;
			path_to_import_file = c->get_source_dir();
		}
		else if (parsing_import) {
			if (previous_token.empty()) {
				if (it->str == "all") {
					symbol = "*";
				}
				else {
					previous_token = it->str;
				}
			}
			else if (it->str == "=") {
				throw Unsupported_Feature_Exception{ "Importing an entity with assigning a different identifier is not supported." };
			}
			else if (it->str != ";") {
				if (previous_token != ".") {
					path_to_import_file.append("\\").append(previous_token);
				}
				previous_token = it->str;
			}
			else if (it->str == ";") {
				if (symbol.empty()) {
					symbol = previous_token;
				}
				else {
					if (previous_token != ".") {
						path_to_import_file.append("\\").append(previous_token);
					}
					else {
						throw Wrong_Token_Exception{ "Unexpected token." };
					}
				}
				path_to_import_file.append(".cal");
				Converter_RVC_Cpp::converted_unit_file output =
					Converter_RVC_Cpp::convert_unit_file(conversion_data.get_symbol_map(),
														conversion_data, c->get_source_dir(),
														path_to_import_file, symbol, "\t");
				conversion_data.add_var_code(output.code);
				conversion_data.add_declarations(output.declarations);
				parsing_import = false;
				path_to_import_file.clear();
				symbol.clear();
				path_to_import_file.append(c->get_source_dir());
			}
		}
	}
}

void IR::Actor::transform_IR(void) {

#ifdef DEBUG_IR_TRANSFORMATION
	printf("IR transformation of %s.\n", class_name.c_str());
#endif

	Tokenizer token_producer{code};
	Token t = token_producer.get_next_Token();
	token_producer.set_context(Converter_RVC_Cpp::Context::Import);

	import_buffers.push_back(Import_Buffer{ t,token_producer });

	for (auto it = import_buffers.begin(); it != import_buffers.end(); ++it) {
		convert_import(*it);
	}
	token_producer.set_context(Converter_RVC_Cpp::Context::Actor_Head);

	// t now points at actor
	t = token_producer.get_next_Token();
	//t now points at the actor name - 
	actor_name = t.str;
	t = token_producer.get_next_Token();
	// now pointing at the parameter list
	param_buffers.push_back(Parameter_Buffer{ t, token_producer });
	// now pointing at the buffer/port definitions
	parse_buffer_access(t, token_producer);

	// now pointing at :, skip
	t = token_producer.get_next_Token();
	token_producer.set_context(Converter_RVC_Cpp::Context::Actor_Body);

	while ((t.str != "end") && (t.str != "endactor")) {
		if ((t.str == "int") || (t.str == "uint") || (t.str == "String")
			|| (t.str == "bool") || (t.str == "half") || (t.str == "float"))
		{
			var_buffers.push_back(Var_Buffer{ t, token_producer });
		}
		else if (t.str == "List") {
			var_buffers.push_back(Var_Buffer{ t, token_producer });
		}
		else if (t.str == "@native") {
			native_buffers.push_back(Native_Buffer{ t, token_producer });
		}
		else if (t.str == "function") {
			method_buffers.push_back(Method_Buffer{ t, token_producer });
		}
		else if (t.str == "procedure") {
			method_buffers.push_back(Method_Buffer{ t, token_producer });
		}
		else if (t.str == "schedule") {
			parse_schedule_fsm(t, token_producer);
		}
		else if (t.str == "priority") {
			parse_priorities(t, token_producer);
		}
		else if (t.str == "") {
			throw Wrong_Token_Exception{ "Unexpected End of File." };
		}
		else {//action
			token_producer.set_context(Converter_RVC_Cpp::Context::Action_Head);
			//actions are buffered because it is not clear at this point if there is a fsm or state variables
			buffered_actions.push_back(Action_Buffer{ t,token_producer });
			token_producer.set_context(Converter_RVC_Cpp::Context::Actor_Body);
		}
	}

	// Now determine the token rates
	for (auto it = buffered_actions.begin(); it != buffered_actions.end(); ++it) {
		parse_action(*it);
	}


#ifdef DEBUG_IR_TRANSFORMATION
	printf("Number of actions: %llu.\n", buffered_actions.size());
	for (auto it = in_buffers.begin(); it != in_buffers.end(); ++it) {
		printf("Input Port: %s, type: %s.\n", it->buffer_name.c_str(), it->type.c_str());
	}
	for (auto it = out_buffers.begin(); it != out_buffers.end(); ++it) {
		printf("Output Port: %s, type: %s.\n", it->buffer_name.c_str(), it->type.c_str());
	}
	for (auto it = priorities.begin(); it != priorities.end(); ++it) {
		printf("Priority: %s > %s;\n", it->action_high.c_str(), it->action_low.c_str());
	}
	if (!initial_state.empty()) {
		printf("Initial state: %s\n", initial_state.c_str());
	}
	for (auto it = fsm.begin(); it != fsm.end(); ++it) {
		printf("FSM: %s(%s) -> %s;\n", it->state.c_str(), it->action.c_str(), it->next_state.c_str());
	}
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		printf("Found action: %s, guard: %s\n", (*it)->get_name().c_str(), (*it)->get_guard().c_str());
		for (auto i_it = (*it)->get_in_buffers().begin();
				  i_it != (*it)->get_in_buffers().end();
			      ++i_it) {
			printf("Input buffer: %s, tokenrate: %d\n", i_it->buffer_name.c_str(), i_it->tokenrate);
		}
		for (auto o_it = (*it)->get_out_buffers().begin();
			o_it != (*it)->get_out_buffers().end();
			++o_it) {
			printf("Output buffer: %s, tokenrate: %d\n", o_it->buffer_name.c_str(), o_it->tokenrate);
		}
	}

	for (auto it = method_buffers.begin(); it != method_buffers.end(); ++it) {
		it->print_buffer();
	}

	for (auto it = var_buffers.begin(); it != var_buffers.end(); ++it) {
		it->print_buffer();
	}
	for (auto it = buffered_actions.begin(); it != buffered_actions.end(); ++it) {
		it->print_buffer();
	}
	for (auto it = conversion_data.get_symbol_map().begin(); it != conversion_data.get_symbol_map().end(); ++it) {
		std::cout << "Symbol: " << it->first << " Value: " << it->second << std::endl;
	}

	printf("IR transformation of %s done.\n", class_name.c_str());
#endif
}