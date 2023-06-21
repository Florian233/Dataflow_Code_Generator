#include "Action_Conversion.hpp"
#include "Conversion/Converter_RVC_Cpp.hpp"
#include "Conversion/Unsupported.hpp"
#include "Tokenizer/Tokenizer.hpp"
#include <tuple>

/*
	This function takes a token pointing at the start of the list comprehension, the corresponding Action_Buffer,
	the name of the FIFO the list comprehension is used for and the prefix for each line of code as arguments.
	This function is only used for list comprehensions that occur in FIFOs, all other list comprehensions are handled in the Converter_RVC_Cpp.
	The list comprehension is converted to a loop or multiple loops in pure C++ without SYCL. For list comprehension in relation with SYCL there is another function.
	The assumption is, list comprehension can only occur in output FIFOs, because in input FIFOs it wouldn't make any sense.
	The function returns the generated C++ as a string.
	After the function the Token reference should point at the closing ] of the list comprehension.
*/
static std::string convert_list_comprehension(
	Token& t,
	Token_Container& token_producer,
	std::string var_name,
	std::string prefix,
	std::map<std::string, std::string>& global_map,
	std::map<std::string, std::string>& local_map)
{
	std::string output;
	std::string var_iterator = Converter_RVC_Cpp::find_unused_name(global_map, local_map);
	t = token_producer.get_next_Token();//drop [
	output.append(prefix + "unsigned " + var_iterator + " = 0;\n");
	std::string command{ "\t" + var_name + "[" + var_iterator + "++] = " };
	//everything before the : is the body of the loop(s)
	std::string tmp;
	while ((t.str != ":") && (t.str != "]")) {
		if (t.str == "if") {
			auto buf = Converter_RVC_Cpp::convert_inline_if(t, token_producer);
			tmp.append(buf.first);
		}
		else if (t.str == "[") {
			tmp.append(Converter_RVC_Cpp::convert_brackets(t, token_producer, false, global_map, local_map, prefix).first);
		}
		else {
			tmp.append(t.str);
			t = token_producer.get_next_Token();
		}
	}
	if (t.str == ":") {
		command.append(tmp + ";\n");
		t = token_producer.get_next_Token();// drop :
		//loop head(s)
		std::string head{};
		std::string tail{};
		while (t.str != "]") {
			if ((t.str == "for") || (t.str == "foreach")) {
				head.append(prefix + "for(");
				t = token_producer.get_next_Token();
				if ((t.str == "uint") || (t.str == "int") || (t.str == "String") || (t.str == "bool")
					|| (t.str == "half") || (t.str == "float"))
				{
					head.append(Converter_RVC_Cpp::convert_type(t, token_producer, global_map));
					head.append(" " + t.str + " = ");
					std::string var_name{ t.str };
					t = token_producer.get_next_Token(); //in
					t = token_producer.get_next_Token(); //start value
					head.append(t.str + ";");
					t = token_producer.get_next_Token(); //..
					t = token_producer.get_next_Token(); //end value
					head.append(var_name + " <= " + t.str + ";" + var_name + "++){\n");
					t = token_producer.get_next_Token();
				}
				else {
					//no type, directly variable name, indicates foreach loop
					head.append("auto " + t.str);
					t = token_producer.get_next_Token();//in
					head.append(":");
					t = token_producer.get_next_Token();
					if ((t.str == "[") || (t.str == "{")) {
						t = token_producer.get_next_Token();
						head.append("{");
						while (t.str != "]") {
							head.append(t.str);
						}
						head.append("}");
						t = token_producer.get_next_Token();
					}
				}
				tail.append(prefix + "}\n");
				if (t.str == ",") {//a komma indicates a further loop head, thus the next token has to be inspected
					t = token_producer.get_next_Token();
				}
			}
		}
		output.append(head);
		output.append(prefix + command);
		output.append(tail);
	}
	else if (t.str == "]") {
		tmp.insert(0, "{");
		tmp.append("}");
		std::string unused_var_name = Converter_RVC_Cpp::find_unused_name(global_map, local_map);
		command.append(unused_var_name + ";\n");
		output.append(prefix + "for( auto " + unused_var_name + ":" + tmp + ") {\n");
		output.append(prefix + command);
		output.append(prefix + "}\n");
	}
	return output;
}

/*
	This function has to be used if something like: int a[2] = b<3?[5,b]:[0,0] is used in the part of the action where the ports/FIFOs are accessed.
	C++ doesn't allow list assignments without initialization. Thus, this must be converted to a for loop surrounded by condition checking (if...else...).
	The result is returned as a string. This function is meant to be used if SYCL is NOT used. Otherwise the elements won't be stored in the FIFOs and undefined variables will be used!
*/
static std::string convert_inline_if_with_list_assignment(
	Token& t,
	Token_Container& token_producer,
	std::map<std::string, std::string>& global_map,
	std::map<std::string, std::string>& local_map,
	std::string prefix,
	std::string var_name)
{
	std::string condition;
	std::string expression1;
	std::string expression2;
	std::string output;
	//Condition
	while (t.str != "?") {
		condition.append(t.str + " ");
		t = token_producer.get_next_Token();
	}
	t = token_producer.get_next_Token(); // skip "?"
	//if body
	int count{ 1 };
	bool nested{ false };
	while (count != 0) {
		if (t.str == "?") {
			++count;
			nested = true;
		}
		else if (t.str == ":") {
			--count;
		}
		expression1.append(t.str + " ");
		t = token_producer.get_next_Token();
	}
	expression1.erase(expression1.size() - 2, 2);//remove last :
	if (nested) {
		Tokenizer tok{ expression1 };
		Token tok_token = tok.get_next_Token();
		expression1 = convert_inline_if_with_list_assignment(tok_token, tok, global_map, local_map, prefix + "\t", var_name);
	}
	else {
		//auf listenzuweiseung prüfen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
		if (expression1[0] == '[') {
			Tokenizer tok{ expression1 };
			Token tok_token = tok.get_next_Token();
			expression1 = convert_list_comprehension(tok_token, tok, var_name, prefix + "\t", global_map, local_map);
		}
		else {
			expression1 = prefix + "\t" + var_name + " = " + expression1 + ";\n";
		}
	}

	count = 1;
	nested = false;
	while (count != 0) {
		if (t.str == "?") {
			++count;
			nested = true;

		}
		else if (t.str == ":") {
			--count;
		}
		else if (t.str == "") {
			break;
		}
		expression2.append(t.str + " ");
		t = token_producer.get_next_Token();
	}
	if (nested) {
		Tokenizer tok{ expression2 };
		Token t = tok.get_next_Token();
		expression2 = convert_inline_if_with_list_assignment(t, tok, global_map, local_map, prefix + "\t", var_name);
	}
	else {
		//auf listenzuweiseung prüfen, wenn ja mit convert_list_comprehension in C++ Code umwandeln
		if (expression2[0] == '[') {
			Tokenizer tok{ expression2 };
			Token t = tok.get_next_Token();
			expression2 = convert_list_comprehension(t, tok, var_name, prefix + "\t", global_map, local_map);
		}
		else {
			expression2 = prefix + "\t" + var_name + " = " + expression2 + ";\n";
		}
	}

	//Build expression
	output.append(prefix + "if(" + condition + ") {\n");
	output.append(expression1);
	output.append(prefix + "}\n");
	output.append(prefix + "else {\n");
	output.append(expression2);
	output.append(prefix + "}\n");
	return output;
}


/*
	This function takes a token that points to part of the action where operations are performed (after the fifo access part),
	the corresponding Action_Buffer, the local map for this action and the prefix for each line of code as inputs.
	The function converts these statements to C++ and returns this code as a string.
	All symbol declarations/definitions are inserted into the local map.
*/
static std::string convert_action_body(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string> local_symbol_map,
	std::string prefix = "")
{
	std::string output;
	while ((t.str != "end") && (t.str != "endaction")) {
		if ((t.str == "var") || (t.str == "do")) {
			t = token_producer.get_next_Token();
		}
		else if ((t.str == "for") || (t.str == "foreach")) {
			output.append(Converter_RVC_Cpp::convert_for(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, false, prefix));
		}
		else if (t.str == "while") {
			output.append(Converter_RVC_Cpp::convert_while(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, false, prefix));
		}
		else if (t.str == "if") {
			output.append(Converter_RVC_Cpp::convert_if(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, false, prefix));
		}
		else {
			output.append(Converter_RVC_Cpp::convert_expression(t, token_producer,
				actor_data.get_symbol_map(), local_symbol_map, "*", false, prefix));
		}
	}
	return output;
}

/*
	This function converts the access part to input fifos.
	The token points at the start of this part and it has to contain the first fifo name or ==> if there is not input fifo access,action buffer has to be the
	corresponding buffer to the token. Also the function takes the action information object for this action, the local map of this action and the prefix for each
	line of code as arguments.
	Every FIFO name with the number of consumed tokens is inserted into the action information object and every symbol declaration is inserted into the local map.
	The function returns the generated code in a string object.
*/
static std::tuple<std::string, std::string> convert_input_FIFO_access(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string>& local_map,
	std::string prefix,
	std::string method_name,
	bool input_channel_parameters,
	std::set<std::string> unused_in_channels)
{
	//token contains start of the fifo part - first fifo name
	std::string output;
	std::string definitions;
	std::string parameters;
	while (t.str != "==>") {
		std::string name{ t.str };
		bool unused_channel = (unused_in_channels.find(name) != unused_in_channels.end());
		t = token_producer.get_next_Token(); // :
		t = token_producer.get_next_Token(); // [
		std::vector<std::pair<std::string, bool>> consumed_element_names; // store elements first, because there can be a repeat
		t = token_producer.get_next_Token(); //start of token name part
		while (t.str != "]") {
			std::string expr{};
			bool first{ true };
			bool already_known_variable{ false };
			while ((t.str != ",") && (t.str != "]")) {
				if (first) {
					//check if variable name is already known and therefore there is no need to define it again!
					first = false;
					if (actor_data.get_symbol_map().contains(t.str)) {
						already_known_variable = true;
					}
				}
				if (t.str == "[") {
					while (t.str != "]") {
						expr.append(t.str);
						t = token_producer.get_next_Token();
					}
					expr.append(t.str);
					t = token_producer.get_next_Token();
				}
				else {
					expr.append(t.str);
					t = token_producer.get_next_Token();
				}
			}
			if (t.str == ",") {
				t = token_producer.get_next_Token();
			}
			consumed_element_names.push_back(std::make_pair(expr, already_known_variable));
		}
		t = token_producer.get_next_Token(); // drop ]
		if (t.str == "repeat") {
			t = token_producer.get_next_Token(); //repeat count
			std::string repeat_count_expression;
			while ((t.str != ",") && (t.str != "==>")) {
				repeat_count_expression.append(t.str + " ");
				t = token_producer.get_next_Token();
			}
			int repeat_count = Converter_RVC_Cpp::evaluate_constant_expression(repeat_count_expression,
				actor_data.get_symbol_map(), local_map);

			std::string repeat_iterator;
			repeat_iterator = Converter_RVC_Cpp::find_unused_name(actor_data.get_symbol_map(), local_map);
			if (!unused_channel) {
				output.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") { \n");

				if (input_channel_parameters) {
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "$param");
				}
			}

			//create expressions to read the tokens from the fifo
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it) {
				if (std::get<1>(*it)) {
					// Variable already declared, no declaration needed
				}
				else {
					definitions.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it) + "[" + std::to_string(repeat_count) + "];\n");
				}
				if (unused_channel) {
					continue;
				}

				if (input_channel_parameters) {
					output.append("\t" + prefix + std::get<0>(*it) + "[" + repeat_iterator + "] = " + name + "$param[" + repeat_iterator + "];\n");
				}
				else {
					output.append("\t" + prefix + std::get<0>(*it) + "[" + repeat_iterator + "] = " + name + "->read();\n");
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(consumed_element_names.size()) * repeat_count;
			d.in = true;
			d.parameter_generated = input_channel_parameters && !unused_channel;
			d.repeat = true;
			d.is_pointer = true;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it) {
				d.var_names.push_back(std::get<0>(*it));
			}
			actor_data.add_scheduler_data(method_name, d);

			if (!unused_channel) {
				output.append(prefix + "}\n");
			}
		}
		else {
			std::string channel_iterator;
			if (input_channel_parameters && !unused_channel) {
				if (consumed_element_names.size() > 1) {
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "$param");
					channel_iterator = Converter_RVC_Cpp::find_unused_name(actor_data.get_symbol_map(), local_map);
					output.append(prefix + "unsigned " + channel_iterator + " = 0;\n");
				}
				else {
					if (get<1>(consumed_element_names[0])) {
						parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " " + name + "$param");
						output.append(prefix + get<0>(consumed_element_names[0]) + " = " + name + "$param;\n");
					}
					else {
						parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " " + get<0>(consumed_element_names[0]));
					}
				}
			}

			//no repeat, every element in the fifo brackets consumes one element
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it) {
				if (input_channel_parameters && !unused_channel) {
					if (consumed_element_names.size() == 1) {
						continue;
					}
					if (std::get<1>(*it)) {
						output.append(prefix + std::get<0>(*it) + " = " + name + "$param["+ channel_iterator  +"];\n");
					}
					else {
						output.append(prefix + "auto " + std::get<0>(*it) + " = " + name + "$param["+ channel_iterator  +"];\n");
					}
				}
				else {
					if (std::get<1>(*it)) {
						if (!unused_channel) {
							output.append(prefix + std::get<0>(*it) + " = " + name + "->read();\n");
						}
					}
					else {
						output.append(prefix + "auto " + std::get<0>(*it));
						if (unused_channel) {
							output.append(";\n");
						}
						else {
							output.append(" = " + name + "->read();\n");
						}
					}
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(consumed_element_names.size());
			d.in = true;
			d.parameter_generated = input_channel_parameters && !unused_channel;
			d.repeat = false;
			d.is_pointer = consumed_element_names.size() > 1;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			for (auto it = consumed_element_names.begin(); it != consumed_element_names.end(); ++it) {
				d.var_names.push_back(std::get<0>(*it));
			}
			actor_data.add_scheduler_data(method_name, d);
		}
		if (t.str == ",") {// another fifo access, otherwise it has to be ==> and the loop terminates
			t = token_producer.get_next_Token();
			if (input_channel_parameters && !unused_channel) {
				parameters.append(",");
			}
		}
	}
	return std::make_tuple(parameters, definitions + output);
}

/*
	The functions takes a token pointing at the first fifo name of the output part and the corresponding action buffer, the action information object
	for this action, the local map of this action and the prefix for each line of code as input parameters.
	The fifo accesses are converted to pure C++ code and this code is returned as a string.
	Additionally, each fifo name and number of consumed/produces tokens in this access are inserted into the output fifo list of the action information list.
	All defined symbols are inserted into the local map.
*/
static std::tuple<std::string, std::string> convert_output_FIFO_access(
	Token& t,
	Action_Buffer& token_producer,
	Actor_Conversion_Data& actor_data,
	std::map<std::string, std::string>& local_map,
	std::string prefix,
	std::string method_name,
	bool output_channel_parameters,
	std::set<std::string> unused_out_channels)
{
	//token contains start of output fifo part - first fifo name
	std::string output;
	std::string definitions;
	std::string parameters;
	while ((t.str != "do") && (t.str != "guard") && (t.str != "var") && (t.str != "end")
		&& (t.str != "endaction"))
	{
		unsigned accessor_counter = 0;
		std::string name{ t.str };
		bool unused_channel = (unused_out_channels.find(name) != unused_out_channels.end());
		std::vector<std::tuple<std::string,bool>> output_fifo_expr; //bool = true indicates that define shall be added, e.g. for list comprehension
		t = token_producer.get_next_Token(); //:
		t = token_producer.get_next_Token(); //[
		t = token_producer.get_next_Token();
		while (t.str != "]") {
			std::string expr;
			bool add_define{ false };
			std::string accessor_var = name + "$" + std::to_string(accessor_counter);
			if (t.str == "[") {
				//Found List comprehension
				output.append(convert_list_comprehension(t, token_producer, accessor_var,
					prefix, actor_data.get_symbol_map(), local_map));
				t = token_producer.get_next_Token(); //drop last ] of the list comprehension
				expr = accessor_var;
				add_define = true;
			}
			else {
				while ((t.str != ",") && (t.str != "]")) {
					if (t.str == "[") {
						while (t.str != "]") {
							expr.append(t.str);
							t = token_producer.get_next_Token();
						}
						expr.append(t.str);
						t = token_producer.get_next_Token();
					}
					else if (t.str == "(") {
						while (t.str != ")") {
							expr.append(t.str);
							t = token_producer.get_next_Token();
						}
						expr.append(t.str);
						t = token_producer.get_next_Token();
					}
					else if (t.str == "if") {
						auto tmp = Converter_RVC_Cpp::convert_inline_if(t, token_producer);
						if (tmp.second) {
							Tokenizer tok{ tmp.first };
							Token tok_token = tok.get_next_Token();
							output.append(convert_inline_if_with_list_assignment(tok_token, tok,
								actor_data.get_symbol_map(), local_map, prefix, accessor_var));
							add_define = true;
							expr.append(accessor_var);
						}
						else {
							expr.append(tmp.first);
						}
					}
					else {
						expr.append(t.str);
						t = token_producer.get_next_Token();
					}
				}
				//output.append(prefix + accessor_var + " =" + expr + ";\n");
			}
			if (t.str == ",") {
				t = token_producer.get_next_Token();
				++accessor_counter;
			}
			output_fifo_expr.push_back(std::make_tuple(expr, add_define));
		}

		t = token_producer.get_next_Token();
		if (t.str == "repeat") {
			t = token_producer.get_next_Token();//contains repeat count
			std::string repeat_count_expression;
			while ((t.str != ",") && (t.str != "do") && (t.str != "var") && (t.str != "end")
				&& (t.str != "endaction") && (t.str != "guard"))
			{
				repeat_count_expression.append(t.str + " ");
				t = token_producer.get_next_Token();
			}

			int repeat_count = Converter_RVC_Cpp::evaluate_constant_expression(repeat_count_expression,
				actor_data.get_symbol_map(), local_map);

			std::string repeat_iterator;
			std::string in_repeat_iterator;
			if (!unused_channel) {
				repeat_iterator = Converter_RVC_Cpp::find_unused_name(actor_data.get_symbol_map(), local_map);
				output.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") {\n");

				in_repeat_iterator = Converter_RVC_Cpp::find_unused_name(actor_data.get_symbol_map(), local_map);
				if (output_channel_parameters) {
					parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "$param");
					output.append(prefix + "\t" + in_repeat_iterator + " = " + repeat_iterator + " * " + std::to_string(output_fifo_expr.size()) + ";\n");
				}
			}
			for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it) {
				if (std::get<1>(*it)) {
					// in case of the unused channel it could also be an unconnected channel,
					// hence, we must generate a variable as it might be used before.
					// The compiler will optimize this anyhow.
					definitions.append(prefix + actor_data.get_channel_name_type_map()[name] + " " + std::get<0>(*it) + "[" + std::to_string(repeat_count) + "];\n");
				}
				if (unused_channel) {
					continue;
				}

				if (output_channel_parameters) {
					output.append(prefix + "\t" + name + "$param[" + in_repeat_iterator + "++] = " + std::get<0>(*it) + "[" + repeat_iterator + "];\n");
				}
				else {
					output.append(prefix + "\t" + name + "->write(" + std::get<0>(*it) + "[" + repeat_iterator + "]);\n");
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(output_fifo_expr.size()) * repeat_count;
			d.in = false;
			d.parameter_generated = output_channel_parameters && !unused_channel;
			d.repeat = true;
			d.is_pointer = true;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			//ignore var_names here, as the output is written to the channel anyhow and not required for guards
			actor_data.add_scheduler_data(method_name, d);
			
			if (!unused_channel) {
				output.append(prefix + "}\n");
			}
		}
		else {
			if (!unused_channel) {
				/* Ignore the add_define as it is only a scalar value that can written directly to the output */
				std::string channel_iterator;
				if (output_channel_parameters && !unused_channel) {
					if (output_fifo_expr.size() == 1) {
						parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " &" + name + "$param");
					}
					else {
						parameters.append("\n" + prefix + actor_data.get_channel_name_type_map()[name] + " *" + name + "$param");
						channel_iterator = Converter_RVC_Cpp::find_unused_name(actor_data.get_symbol_map(), local_map);
						output.append(prefix + "unsigned " + channel_iterator + " = 0;\n");
					}
				}

				for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it) {

					if (output_channel_parameters) {
						if (output_fifo_expr.size() == 1) {
							output.append(prefix + name + "$param = " + get<0>(*it) + ";\n");
						}
						else {
							output.append(prefix + name + "$param[" + channel_iterator + "++] = " + get<0>(*it) + ";\n");
						}
					}
					else {
						output.append(prefix + name + "->write(" + get<0>(*it) + ");\n");
					}
				}
			}

			Scheduling::Channel_Schedule_Data d;
			d.channel_name = name;
			d.elements = static_cast<unsigned>(output_fifo_expr.size());
			d.in = false;
			d.repeat = false;
			d.parameter_generated = output_channel_parameters && !unused_channel;
			d.is_pointer = output_fifo_expr.size() > 1;
			d.type = actor_data.get_channel_name_type_map()[name];
			d.unused_channel = unused_channel;
			//ignore var_names here, as the output is written to the channel anyhow and not required for guards
			actor_data.add_scheduler_data(method_name, d);
		}
		if (t.str == ",") {// another fifo access, otherwise it has to be var or do  and the loop terminates
			t = token_producer.get_next_Token();
			if (output_channel_parameters && !unused_channel) {
				parameters.append(",");
			}
		}
	}
	return std::make_tuple(parameters, definitions + output);
}

/*
	This function takes an action buffer (index at 0), a action information object and the prefix for each line of code as arguments.
	The tokens in the action buffer will be converted to pure C++ code.
	Additionally, the name, tag, the name of the generated method and the fifo names and the consumed/produced tokens are inserted into the action information object.
	The generated C++ code is return in a string.
	The scheduling conditions (guards, fifo sizes) are inserted into the scheduling condition map of the class.

	This function also appends the declaration of the generated function to the header_output class variable and the
	complete function with class qualifier to the source_output class variable.
*/
std::string convert_action(
	IR::Action* action,
	Action_Buffer* token_producer,
	Actor_Conversion_Data& actor_data,
	bool input_channel_parameters,
	bool output_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::set<std::string> unused_out_channels,
	std::string prefix)
{
    std::map<std::string, std::string> local_symbol_map;
	std::string output{ };
	std::string end_of_output;
	std::string middle_of_output;
	Token t = token_producer->get_next_Token();
	std::string method_name;

	if (t.str == "action") {//action has no name
		method_name = action->get_name();
	}
	else if (t.str == "initialize") {
		method_name = "init";
		actor_data.set_init();
	}
	else {
		std::string action_tag;
		std::string action_name{ "" };

		action_tag = t.str;
		t = token_producer->get_next_Token(); // : or .
		if (t.str == ".") {
			t = token_producer->get_next_Token();
			while (t.str != ":") {
				if (t.str == ".") {//dots cannot be used in c++ 
					action_name.append("$");
				}
				else {
					action_name.append(t.str);
				}
				t = token_producer->get_next_Token();
			}
		}
		t = token_producer->get_next_Token(); // action
		if (t.str == "initialize") {// init action can have a name, dont know why
			action_tag = "initialize";
			actor_data.set_init();
		}
		method_name = action_tag + "$" + action_name;
	}

	// Add it to the list to inform the scheduler about this action even it doesn't read/write channels
	actor_data.init_action_scheduler_data(method_name);
	action->set_function_name(method_name);

	actor_data.get_symbol_map()[method_name] = "";
	//create function head
	output.append(prefix + "void " + method_name + "(");
	if (!input_channel_parameters && !output_channel_parameters) {
		output.append("void) { \n");
	}
	//convert fifo access
	t = token_producer->get_next_Token();//start of the fifo part - fifo name

	bool insert_separator{ false };
	bool added_parameters{ false };
	while (t.str != "==>") {
		//input fifos
		std::tuple<std::string, std::string> tmp = convert_input_FIFO_access(t, *token_producer, actor_data, local_symbol_map,
		prefix + "\t", method_name, input_channel_parameters, unused_in_channels);
		middle_of_output.append(get<1>(tmp));
		output.append(get<0>(tmp));
		insert_separator = input_channel_parameters;
		if (input_channel_parameters && !get<0>(tmp).empty()) {
			added_parameters = true;
		}
	}
	t = token_producer->get_next_Token(); //get next token, that should contain first output fifo name, if any are accessed
	while ((t.str != "do") && (t.str != "guard") && (t.str != "var") && (t.str != "end")
		&& (t.str != "endaction"))
	{
		//output fifos
		std::tuple<std::string, std::string> tmp = convert_output_FIFO_access(t, *token_producer, actor_data, local_symbol_map,
			prefix + "\t", method_name, output_channel_parameters, unused_out_channels);

		if (output_channel_parameters) {
			if (!std::get<0>(tmp).empty()) {
				added_parameters = true;
				if (insert_separator) {
					output.append(", ");
				}
			}
			output.append(get<0>(tmp));
		}
		end_of_output.append(get<1>(tmp));
	}

	if (input_channel_parameters || output_channel_parameters) {
		if (added_parameters) {
			output.append(")\n" + prefix +"{\n");
		}
		else {
			output.append("void) {\n");
		}
	}
	output.append(middle_of_output);

	//read guards
	if (t.str == "guard") {
		std::string guard;
		t = token_producer->get_next_Token();
		while ((t.str != "var") && (t.str != "do") && (t.str != "end") && (t.str != "endaction")) {
			t = token_producer->get_next_Token();
		}
	}
	//convert the body of the action
	if ((t.str != "end") && (t.str != "endaction")) {
		output.append(convert_action_body(t, *token_producer, actor_data, local_symbol_map, prefix + "\t"));
	}

	return output + end_of_output + prefix + "}\n";
}