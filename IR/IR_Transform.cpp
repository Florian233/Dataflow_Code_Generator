#include "Actor.hpp"
#include <string>
#include <algorithm>
#include "Config/config.h"
#include "Conversion/Conversion.hpp"
#include "Reader/Reader.hpp"
#include <set>


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
			in_buffers.push_back(Buffer_Access{ name, std::vector<std::string>{}, 0, index, type });
			conversion_data.add_channel(name, type);
			add = false;
			++index;
			t = token_producer.get_next_token();
		}
		else {
			type = Conversion_Helper::read_type(t, token_producer, conversion_data.get_symbol_map());
			name = t.str;
			t = token_producer.get_next_token();
			add = true;
		}
	}

	if (add) {
		in_buffers.push_back(Buffer_Access{ name, std::vector<std::string>{}, 0, index, type });
		conversion_data.add_channel(name, type);
		add = false;
	}
	index = 0;

	// now pointing at ==>
	t = token_producer.get_next_token();

	while (t.str != ":") {
		if (t.str == ",") {
			out_buffers.push_back(Buffer_Access{ name, std::vector<std::string>{}, 0, index, type });
			conversion_data.add_channel(name, type);
			add = false;
			++index;
			t = token_producer.get_next_token();
		}
		else {
			type = Conversion_Helper::read_type(t, token_producer, conversion_data.get_symbol_map());
			name = t.str;
			add = true;
			t = token_producer.get_next_token();
		}
	}
	if (add) {
		out_buffers.push_back(Buffer_Access{ name, std::vector<std::string>{}, 0, index, type });
		conversion_data.add_channel(name, type);
	}
}

void IR::Actor::parse_schedule_fsm(Token& t, Tokenizer& token_producer) {

	//token contains schedule 
	t = token_producer.get_next_token(); //contains fsm
	t = token_producer.get_next_token(); //contains starting state
	initial_state.append(t.str);
	t = token_producer.get_next_token(); //contains :
	t = token_producer.get_next_token(); //start of the fsm body
	while (t.str != "end") {
		std::string state{ t.str };
		t = token_producer.get_next_token(); // ( 
		//there can be multiple actions to be fired to trigger this transition!
		std::vector<std::string> vector_action_to_fire;
		while (t.str != ")") {
			t = token_producer.get_next_token();
			std::string action{ t.str };
			t = token_producer.get_next_token();
			while ((t.str != ",") && (t.str != ")")) {
				if (t.str == ".") {
					action.append("_");
				}
				else {
					action.append(t.str);
				}
				t = token_producer.get_next_token();
			}
			vector_action_to_fire.push_back(action);
		}
		t = token_producer.get_next_token(); // -->
		t = token_producer.get_next_token();
		std::string next_state{ t.str };
		t = token_producer.get_next_token(); // ;
		t = token_producer.get_next_token();
		//add all transitions to corresponding class members
		for (auto it = vector_action_to_fire.begin(); it != vector_action_to_fire.end(); ++it) {
			fsm.push_back(FSM_Entry{state, *it , next_state});
		}
	}

	//move one token past the end of the FSM block
	t = token_producer.get_next_token();
}

void IR::Actor::parse_priorities(Token& t, Tokenizer& token_producer) {
	t = token_producer.get_next_token();
	std::vector<std::string> greater_actions;
	while (t.str != "end") {
		if (t.str == ";") {
			t = token_producer.get_next_token();
		}
		else if (t.str == ">") {
			t = token_producer.get_next_token();
			std::string lesser{ t.str };
			t = token_producer.get_next_token();
			while ((t.str != ">") && (t.str != ";")) {
				if (t.str == ".") {
					lesser.append("_");
				} else {
					lesser.append(t.str);
				}
				t = token_producer.get_next_token();
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
			t = token_producer.get_next_token();
			if (t.str == ".") {
				t = token_producer.get_next_token();
				while (t.str != ">" && t.str != ";") {
					if (t.str == ".") {
						greater.append("_");
					}
					else {
						greater.append(t.str);
					}
					t = token_producer.get_next_token();
				}
			}
			greater_actions.clear();
			greater_actions.push_back(greater);
		}
	}
	//move one token past the end of the block
	t = token_producer.get_next_token();
}

static unsigned anonymous_action_counter{ 0 };

static std::string skip_bracket(Token& t, Action_Buffer& token_producer) {
	std::string ret;
	if (t.str == "[") {
		ret = t.str + " ";
		t = token_producer.get_next_token();
		while (t.str != "]") {
			if (t.str == "[") {
				ret.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "(") {
				ret.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				ret.append(" " + t.str);
				t = token_producer.get_next_token();
			}
		}
		ret.append(t.str);
		t = token_producer.get_next_token();
	}
	else if (t.str == "(") {
		ret = t.str + " ";
		t = token_producer.get_next_token();
		while (t.str != ")") {
			if (t.str == "[") {
				ret.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "(") {
				ret.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				ret.append(" " + t.str);
				t = token_producer.get_next_token();
			}
		}
		ret.append(t.str);
		t = token_producer.get_next_token();
	}
	return ret;
}

void IR::Actor::parse_action(Action_Buffer& token_producer) {
	Token t = token_producer.get_next_token();

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
			t = token_producer.get_next_token();
		}
		//skip :
		t = token_producer.get_next_token();
	}
	else {
		name = "action";
		name.append(std::to_string(anonymous_action_counter));
		++anonymous_action_counter;
	}

	IR::Action* action = new IR::Action{ name, &token_producer };
	actions.push_back(action);

	//skip action
	t = token_producer.get_next_token();

	std::vector<std::string> found_buffers;
	std::set<std::string> in_buffer_vars;
	unsigned index = 0;
	// must be a buffer name or ==> if no input buffer is used
	while (t.str != "==>") {
		std::string buffer_name{};
		std::string var_name{};
		std::vector<std::string> var_names{};
		unsigned count = 1;
		if (t.str != "[") {
			//Buffer name explicitly stated
			buffer_name = t.str;
			t = token_producer.get_next_token();
			// must point to : now
			if (t.str != ":") {
				throw Wrong_Token_Exception{ "Expected \":\" but found \"" + t.str + "\" for buffer "+buffer_name+"." };
			}
			t = token_producer.get_next_token();
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
		t = token_producer.get_next_token();

		while (t.str != "]") {
			if (t.str == ",") {
				var_names.push_back(var_name);
				var_name.clear();
				t = token_producer.get_next_token();
				++count;
			}
			else if (t.str == "[") {
				var_name.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "(") {
				var_name.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				if (!var_name.empty()) {
					var_name.append(" ");
				}
				var_name.append(t.str);
				t = token_producer.get_next_token();
			}
		}
		if (!var_name.empty()) {
			var_names.push_back(var_name);
		}

		// now points at ]
		t = token_producer.get_next_token();
		std::string expression = "";
		bool repeat = false;
		if (t.str == "repeat") {
			t = token_producer.get_next_token();
			repeat = true;
			while ((t.str != ",") && (t.str != "==>")) {
				expression.append(t.str);
				t = token_producer.get_next_token();
			}
			count *= Conversion_Helper::evaluate_constant_expression(expression, conversion_data.get_symbol_map());
		}

		action->add_in_buffer(Buffer_Access{ buffer_name, var_names, count, 0, get_in_port_type(buffer_name), repeat, expression});
		found_buffers.push_back(buffer_name);
		in_buffer_vars.insert(var_name);

		//now points at , or ==>, skip it points at ,
		if (t.str == ",") {
			t = token_producer.get_next_token();
		}
	}

	// Now add all buffers that were not used by this action with tokenrate zero
	for (auto it = in_buffers.begin(); it != in_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			action->add_in_buffer(Buffer_Access{ it->buffer_name, std::vector<std::string>{}, 0 });
		}
	}

	found_buffers.clear();

	index = 0;
	//now points at ==>, skip it
	t = token_producer.get_next_token();

	while ((t.str != "guard") && (t.str != "var") && (t.str != "do")
		&& (t.str != "end") && (t.str != "endaction"))
	{
		std::string buffer_name{};
		std::string var_name{};
		std::vector<std::string> var_names{};
		unsigned count = 1;
		if (t.str != "[") {
			//Buffer name explicitly stated
			buffer_name = t.str;
			t = token_producer.get_next_token();
			// must point to : now
			if (t.str != ":") {
				throw Wrong_Token_Exception{ "Expected \":\" but found \"" + t.str + "\"." };
			}
			t = token_producer.get_next_token();
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
		t = token_producer.get_next_token();

		while (t.str != "]") {
			if (t.str == ",") {
				var_names.push_back(var_name);
				var_name.clear();
				t = token_producer.get_next_token();
				++count;
			}
			else if (t.str == "[") {
				var_name.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "(") {
				var_name.append(skip_bracket(t, token_producer));
			}
			else if (t.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else {
				if (!var_name.empty()) {
					var_name.append(" ");
				}
				var_name.append(t.str);
				t = token_producer.get_next_token();
			}
		}
		if (!var_name.empty()) {
			var_names.push_back(var_name);
		}

		// now points at ]
		t = token_producer.get_next_token();
		bool repeat = false;
		std::string expression = "";
		if (t.str == "repeat") {
			t = token_producer.get_next_token();
			repeat = true;
			while ((t.str != ",") && (t.str != "guard") && (t.str != "var")
				&& (t.str != "do") && (t.str != "end") && (t.str != "endaction"))
			{
				expression.append(t.str);
				t = token_producer.get_next_token();
			}
			count *= Conversion_Helper::evaluate_constant_expression(expression, conversion_data.get_symbol_map());
		}

		action->add_out_buffer(Buffer_Access{ buffer_name, var_names, count, 0, get_out_port_type(buffer_name), repeat, expression});
		found_buffers.push_back(buffer_name);

		//now points at , or ==>, skip it points at ,
		if (t.str == ",") {
			t = token_producer.get_next_token();
		}
	}

	//Now add all buffers that are not used by this action with tokenrate zero
	for (auto it = out_buffers.begin(); it != out_buffers.end(); ++it) {
		auto found = std::find(found_buffers.begin(), found_buffers.end(), it->buffer_name);
		if (found == found_buffers.end()) {
			//not found, add as zero
			action->add_out_buffer(Buffer_Access{ it->buffer_name, std::vector<std::string>{}, 0 });
		}
	}

	while ((t.str != "guard") && (t.str != "end") && (t.str != "actionend")) {
		//skip the tokens until we find the end or the guard
		t = token_producer.get_next_token();
	}

	if (t.str == "guard") {

		// while reading the guard check whether it depends on input tokens.
		// We need this information to check fusion criteria later!
		bool state_guard = true;

		//okay, we found the guard, let's store it somewhere...
		std::string guard{};
		//skip the guard token, we don't need that
		t = token_producer.get_next_token();
		while ((t.str != "var") && (t.str != "do") && (t.str != "end") && (t.str != "endaction")) {
			if (in_buffer_vars.find(t.str) != in_buffer_vars.end()) {
				state_guard = false;
				for (auto q = action->get_in_buffers().begin();
					q != action->get_in_buffers().end(); ++q)
				{
					if (std::find(q->var_names.begin(), q->var_names.end(), t.str) != q->var_names.end()) {
						action->add_guard_dependent_input(q->buffer_name);
					}
				}
			}
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
			t = token_producer.get_next_token();
		}
		action->set_state_guard(state_guard);
		action->set_guard(guard);
	}

	// now reset the buffer as it will be used later on to translate the action to C++ code
	token_producer.reset_buffer();
}

void IR::Actor::convert_import(Import_Buffer& token_producer, Dataflow_Network* dpn) {
	Config* c = c->getInstance();
	std::filesystem::path path_to_import_file{ };
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
					path_to_import_file /= previous_token;
				}
				previous_token = it->str;
			}
			else if (it->str == ";") {
				if (symbol.empty()) {
					symbol = previous_token;
				}
				else {
					if (previous_token != ".") {
						path_to_import_file /= previous_token;
					}
					else {
						throw Wrong_Token_Exception{ "Unexpected token." };
					}
				}
				path_to_import_file += ".cal";
				
				Unit* u = Network_Reader::read_unit(dpn, path_to_import_file);
				u->initialize(dpn);
				this->imported_symbols.push_back(std::make_pair(symbol, u));

				for (auto v = u->get_var_buffers().begin(); v != u->get_var_buffers().end(); ++v) {
					Conversion_Helper::read_constants(*v, this->get_conversion_data().get_symbol_map(), symbol);
				}

				parsing_import = false;
				path_to_import_file.clear();
				symbol.clear();
				path_to_import_file.append(c->get_source_dir());
				previous_token.clear();
			}
		}
	}
}

void IR::Actor::transform_IR(Dataflow_Network* dpn) {
	Config* c = c->getInstance();
	if (c->get_verbose_ir_gen()) {
		std::cout << "IR transformation of " << class_name << "." << std::endl;
	}

	Tokenizer token_producer{ code };
	Token t = token_producer.get_next_token();
	token_producer.set_context(Conversion_Helper::Context::Import);

	import_buffers.push_back(Import_Buffer{ t,token_producer });
#ifdef DEBUG_IR_TRANSFORMATION
	std::cout << "Start Import conversion" << std::endl;
#endif
	for (auto it = import_buffers.begin(); it != import_buffers.end(); ++it) {
		convert_import(*it, dpn);
	}
#ifdef DEBUG_IR_TRANSFORMATION
	std::cout << "Import conversion done" << std::endl;
#endif

	token_producer.set_context(Conversion_Helper::Context::Actor_Head);

	if (this->code.find("@native") != this->code.npos) {
		this->use_native = true;
	}

	// t now points at actor
	t = token_producer.get_next_token();
	//t now points at the actor name - 
	actor_name = t.str;
	t = token_producer.get_next_token();
	// now pointing at the parameter list
	param_buffers.push_back(Parameter_Buffer{ t, token_producer });
	// now pointing at the buffer/port definitions
	parse_buffer_access(t, token_producer);

	// now pointing at :, skip
	t = token_producer.get_next_token();
	token_producer.set_context(Conversion_Helper::Context::Actor_Body);

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
			token_producer.set_context(Conversion_Helper::Context::Action_Head);
			//actions are buffered because it is not clear at this point if there is a fsm or state variables
			buffered_actions.push_back(Action_Buffer{ t,token_producer });
			token_producer.set_context(Conversion_Helper::Context::Actor_Body);
		}
	}
#ifdef DEBUG_IR_TRANSFORMATION
	std::cout << "Start reading of constants" << std::endl;
#endif
	for (auto v = var_buffers.begin(); v != var_buffers.end(); ++v) {
		Conversion_Helper::read_constants(*v, this->get_conversion_data().get_symbol_map(), "*");
	}
#ifdef DEBUG_IR_TRANSFORMATION
	std::cout << "Reading of constants done" << std::endl;
#endif

	// Now determine the token rates
	for (auto it = buffered_actions.begin(); it != buffered_actions.end(); ++it) {
		parse_action(*it);
	}

	if (c->get_verbose_ir_gen()) {
		std::cout << "Number of actions: " << buffered_actions.size() << std::endl;
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
			std::cout << "Found action: " << (*it)->get_name() << ", guard: " << (*it)->get_guard() << std::endl;
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

		std::cout << "IR transformation of " << class_name << " done." << std::endl;
	}
}

static void check_imports(
	IR::Unit* parent,
	IR::Unit* child,
	std::string symbol)
{
	for (auto v = child->get_var_buffers().begin(); v != child->get_var_buffers().end(); ++v) {
		v->reset_buffer();
		if (Conversion_Helper::symbol_name_match(*v, symbol)) {
			parent->add_var_buffer(*v);
		}
	}

	for (auto v = child->get_native_buffers().begin(); v != child->get_native_buffers().end(); ++v) {
		v->reset_buffer();
		if (Conversion_Helper::symbol_name_match(*v, symbol)) {
			parent->add_native_buffer(*v);
		}
	}

	for (auto v = child->get_method_buffers().begin(); v != child->get_method_buffers().end(); ++v) {
		v->reset_buffer();
		if (Conversion_Helper::symbol_name_match(*v, symbol)) {
			parent->add_method_buffer(*v);
		}
	}
}

void IR::Unit::convert_imports(
	Token& t,
	Tokenizer& b,
	IR::Dataflow_Network* dpn)
{
	while (t.str != "unit") {
		if (t.str != "import") {
			throw Wrong_Token_Exception{ "Expected \"import\" but found " + t.str + "." };
		}

		std::filesystem::path path_to_import_file{ };
		std::string symbol{};
		path_to_import_file.append(b.get_next_token().str); //must be part of the path
		Token previous_token = b.get_next_token();
		Token next_token = b.get_next_token();

		if (previous_token.str == "all") {
			symbol = "*";
			previous_token = next_token;
			next_token = b.get_next_token();
		}

		for (;;) {
			if (next_token.str == "") {
				throw Wrong_Token_Exception{ "Unexpected End of File." };
			}
			else if (next_token.str != ";") {
				if (previous_token.str != ".") {
					path_to_import_file /= previous_token.str;
				}
				previous_token = next_token;
				next_token = b.get_next_token();
			}
			else if (next_token.str == ";") {
				if (symbol.empty()) {
					symbol = previous_token.str;
				}
				else {
					if (previous_token.str != ".") {
						path_to_import_file /= previous_token.str;
					}
					else {
						throw Wrong_Token_Exception{ "Found unexpected Token." };
					}
				}
				path_to_import_file += ".cal";
				next_token = b.get_next_token();
				break;
			}
		}
		Unit* u = Network_Reader::read_unit(dpn, path_to_import_file);
		u->initialize(dpn);
		sub_units.push_back(std::make_pair(symbol, u));
	}

	for (auto it = sub_units.begin(); it != sub_units.end(); ++it) {
		check_imports(this, it->second, it->first);
	}
}

void IR::Unit::initialize(IR::Dataflow_Network* dpn) {
	if (initialized) {
		return;
	}
	initialized = true;
	Tokenizer token_producer{ code };
	Token t = token_producer.get_next_token();

	/* skip package */
	while (t.str != "import" && t.str != "unit") {
		t = token_producer.get_next_token();
		if (t.str == "") {
			throw Wrong_Token_Exception{ "Unexpected End of File." };
		}
	}
	if (t.str == "import") {
		convert_imports(t, token_producer, dpn);
	}

	if (t.str != "unit") {
		throw Wrong_Token_Exception{ "Excepted \"unit\" but found " + t.str + "." };
	}

	Token name = token_producer.get_next_token();
	Token doppelpunkt = token_producer.get_next_token();
	t = token_producer.get_next_token();

	while ((t.str != "end") && (t.str != "endunit")) {
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
		else {
			throw Wrong_Token_Exception{ "Unexpected End of File." };
		}
	}
}