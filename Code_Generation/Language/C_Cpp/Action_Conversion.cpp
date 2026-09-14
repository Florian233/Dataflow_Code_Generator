#include "Action_Conversion.hpp"
#include "Converter_RVC_Cpp.hpp"
#include <tuple>
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Data.hpp"
#include "ABI/abi.hpp"
#include "Config/config.h"
#include "common/include/String_Helper.h"
#include "Conversion/Conversion.hpp"
#include <iostream>

/*
	This function converts the access part to input fifos.
	The token points at the start of this part and it has to contain the first fifo name or ==> if there is not input fifo access,action buffer has to be the
	corresponding buffer to the token. Also the function takes the action information object for this action, the local map of this action and the prefix for each
	line of code as arguments.
	Every FIFO name with the number of consumed tokens is inserted into the action information object and every symbol declaration is inserted into the local map.
	The function returns the generated code in a string object.
*/
static std::tuple<std::string, std::string> convert_input_FIFO_access(
	AST::Input_Pattern* input,
	std::string prefix,
	std::string method_name,
	bool input_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& sched_data,
	std::map<std::string, std::string> port_type_map,
	std::map<std::string, std::string> const_map)
{
	Config* c = Config::getInstance();
	//token contains start of the fifo part - first fifo name
	std::string output;
	std::string definitions;
	std::string parameters;
	int repeat_count = 1;
	bool unused_channel = (unused_in_channels.find(input->port.name) != unused_in_channels.end());

	if (input->repeat != nullptr) {
		repeat_count = Conversion_Helper::evaluate_constant_expression(input->repeat, const_map );

		std::string repeat_iterator;
		repeat_iterator = Converter_RVC_Cpp::unused_identifier();
		if (!unused_channel) {
			output.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") { \n");

			if (input_channel_parameters) {
				parameters.append("\n" + prefix + port_type_map[input->port.name] + " *" + input->port.name + "_param");
			}
		}

		//create expressions to read the tokens from the fifo
		for (auto it = input->IDs.begin(); it != input->IDs.end(); ++it) {
			definitions.append(prefix + port_type_map[input->port.name] + " " + it->name + "[" + std::to_string(repeat_count) + "];\n");
			if (unused_channel) {
				continue;
			}

			if (input_channel_parameters) {
				output.append("\t" + prefix + it->name + "[" + repeat_iterator + "] = " + input->port.name + "_param[" + repeat_iterator + "];\n");
			}
			else {
				std::string tmp;
				ABI_CHANNEL_READ(c, tmp, input->port.name)
				output.append("\t" + prefix + it->name + "[" + repeat_iterator + "] = " + tmp + ";\n");
			}
		}

		if (!unused_channel) {
			output.append(prefix + "}\n");
		}
	}
	else {
		std::string channel_iterator;
		if (input_channel_parameters && !unused_channel) {
			if (input->IDs.size() > 1) {
				parameters.append("\n" + prefix + port_type_map[input->port.name] + " *" + input->port.name + "_param");
				channel_iterator = Converter_RVC_Cpp::unused_identifier();
				output.append(prefix + "unsigned " + channel_iterator + " = 0;\n");
			}
			else {
				parameters.append("\n" + prefix + port_type_map[input->port.name] + " " + input->IDs.at(0).name);
			}
		}

		//no repeat, every element in the fifo brackets consumes one element
		for (auto it = input->IDs.begin(); it != input->IDs.end(); ++it) {
			if (input_channel_parameters && !unused_channel) {
				if (input->IDs.size() == 1) {
					continue;
				}
				output.append(prefix + port_type_map[input->port.name] + " " + it->name + " = " + input->port.name + "_param["+ channel_iterator  +"];\n");
			}
			else {
				output.append(prefix + port_type_map[input->port.name] + " " + it->name);
				if (unused_channel) {
					output.append(";\n");
				}
				else {
					std::string tmp;
					ABI_CHANNEL_READ(c, tmp, input->port.name)
					output.append(" = " + tmp + "; \n");
				}
			}
		}
	}

	Scheduling::Channel_Schedule_Data d;
	d.channel_name = input->port.name;
	d.elements = static_cast<unsigned>(input->IDs.size()) * repeat_count;
	d.in = true;
	d.parameter_generated = input_channel_parameters && !unused_channel;
	d.repeat = repeat_count > 1;
	d.is_pointer = d.elements > 1;
	d.type = port_type_map[input->port.name];
	d.unused_channel = unused_channel;
	d.arg = 0;
	for (auto it = input->IDs.begin(); it != input->IDs.end(); ++it) {
		d.var_names.push_back((*it).name);
	}
	sched_data[method_name].push_back(d);

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
	AST::Output_Expression* output,
	std::string prefix,
	std::string method_name,
	std::set<std::string> unused_out_channels,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& scheduledata,
	std::map<std::string, std::string> port_type_map,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::string> const_map)
{
	Config* c = Config::getInstance();
	//token contains start of output fifo part - first fifo name
	std::string output_str;
	std::string definitions;
	std::string parameters;

	unsigned accessor_counter = 0;
	bool unused_channel = (unused_out_channels.find(output->port.name) != unused_out_channels.end());
	int repeat_count = 1;

	std::vector<std::tuple<std::string, bool>> output_fifo_expr; //bool = true indicates that define shall be added, e.g. for list comprehension
	for(auto e : output->expressions) {
		std::string expr;
		bool add_define{ false };
		std::string accessor_var = output->port.name + "_" + std::to_string(accessor_counter);
		if (dynamic_cast<AST::ListComprehension*>(e->child) != nullptr) {
			//Found List comprehension
			auto tmp = Converter_RVC_Cpp::convert_listcomprehension(dynamic_cast<AST::ListComprehension*>(e->child), prefix, accessor_var,
				replacements, const_map);
			output_str.append(tmp.first + tmp.second);
			expr = accessor_var;
			add_define = true;
		}
		else {
			expr = Converter_RVC_Cpp::convert_expression(e, replacements, const_map);
			//output.append(prefix + accessor_var + " =" + expr + ";\n");
		}
		++accessor_counter;
		output_fifo_expr.push_back(std::make_tuple(expr, add_define));
	}

	if (output->repeat != nullptr) {
		repeat_count = Conversion_Helper::evaluate_constant_expression(output->repeat, const_map );

		std::string repeat_iterator;
		if (!unused_channel) {
			repeat_iterator = Converter_RVC_Cpp::unused_identifier();
			output_str.append(prefix + "for (unsigned " + repeat_iterator + " = 0; " + repeat_iterator + " < " + std::to_string(repeat_count) + "; ++" + repeat_iterator + ") {\n");
		}
		for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it) {
			if (std::get<1>(*it)) {
				// in case of the unused channel it could also be an unconnected channel,
				// hence, we must generate a variable as it might be used before.
				// The compiler will optimize this anyhow.
				definitions.append(prefix + port_type_map[output->port.name] + " " + std::get<0>(*it) + "[" + std::to_string(repeat_count) + "];\n");
			}
			if (unused_channel) {
				continue;
			}

			std::string tmp;
			std::string wv = std::get<0>(*it) + "[" + repeat_iterator + "]";
			ABI_CHANNEL_WRITE(c, tmp, wv, output->port.name)
			output_str.append(prefix + "\t" + tmp + ";\n");
		}
		if (!unused_channel) {
			output_str.append(prefix + "}\n");
		}
	}
	else {
		if (!unused_channel) {
			/* Ignore the add_define as it is only a scalar value that can written directly to the output */
			for (auto it = output_fifo_expr.begin(); it != output_fifo_expr.end(); ++it) {
				std::string tmp;
				std::string wv = get<0>(*it);
				ABI_CHANNEL_WRITE(c, tmp, wv, output->port.name)
				output_str.append(prefix + tmp + ";\n");
			}
		}
	}

	Scheduling::Channel_Schedule_Data d;
	d.channel_name = output->port.name;
	d.elements = static_cast<unsigned>(output->expressions.size()) * repeat_count;
	d.in = false;
	d.repeat = repeat_count > 1;
	d.parameter_generated = false;
	d.is_pointer = d.elements > 1;
	d.type = port_type_map[output->port.name];
	d.unused_channel = unused_channel;
	d.arg = 0;
	//ignore var_names here, as the output is written to the channel anyhow and not required for guards
	scheduledata[method_name].push_back(d);

	return std::make_tuple(parameters, definitions + output_str);
}

/*
	This function takes an action buffer (index at 0), a action information object and the prefix for each line of code as arguments.
	The tokens in the action buffer will be converted to pure C++ code.
	Additionally, the name, tag, the name of the generated method and the fifo names and the consumed/produced tokens are inserted into the action information object.
	The generated C++ code is returned in a string.
	The scheduling conditions (guards, fifo sizes) are inserted into the scheduling condition map of the class.

	This function also appends the declaration of the generated function to the header_output class variable and the
	complete function with class qualifier to the source_output class variable.
*/
std::string convert_action(
	IR::Action* action,
	bool input_channel_parameters,
	std::set<std::string> unused_in_channels,
	std::set<std::string> unused_out_channels,
	std::string prefix,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& scheduledata,
	std::string class_name,
	std::map<std::string, std::string> port_type_map,
	std::map<std::string, std::string> const_map,
	std::string additional_code)
{
	std::string output{ prefix };
	std::string end_of_output;
	std::string middle_of_output;
	std::string method_name;
	Config* c = Config::getInstance();

	if (action->is_init()) {
		if (c->get_target_language() == Target_Language::c) {
			method_name = class_name + "_initialize";
		}
		else {
			method_name = "initialize";
		}
	}
	else {
		method_name = action->get_name();
		replace_all_substrings(method_name, ".", "_");
	}

	// Add it to the list to inform the scheduler about this action even it doesn't read/write channels
	action->set_function_name(method_name);
	scheduledata[method_name] = std::vector<Scheduling::Channel_Schedule_Data>();

	bool insert_separator{ false };
	bool added_parameters{ false };
	//create function head

	if (c->get_target_language() == Target_Language::c) {
		if (!action->is_init()) {
			output.append("static ");
		}
		output.append("void " + method_name + "(");
		if (c->get_target_ABI() != Target_ABI::rtos) {
			output.append(class_name + "_t *_g");
			insert_separator = true;
			added_parameters = true;
		}
	}
	else {
		output.append("void " + method_name + "(");
	}
	if (!input_channel_parameters) {
		if (c->get_target_language() == Target_Language::cpp) {
			output.append("void");
		}
		output.append(") {\n");
	}

	for (auto in : action->get_ast()->input_patterns) {
		//input fifos
		std::tuple<std::string, std::string> tmp = convert_input_FIFO_access(in,
		prefix + "\t", method_name, input_channel_parameters, unused_in_channels, scheduledata, port_type_map, const_map);
		middle_of_output.append(get<1>(tmp));
		if (input_channel_parameters && !get<0>(tmp).empty()) {
			added_parameters = true;
			if (insert_separator) {
				output.append(", ");
			}
			insert_separator = input_channel_parameters;
			output.append(get<0>(tmp));
		}
	}

	for (auto out : action->get_ast()->output_expressions) {
		std::tuple<std::string, std::string> tmp = convert_output_FIFO_access(out,
			prefix + "\t", method_name, unused_out_channels, scheduledata, port_type_map,
			replacements, const_map);

		end_of_output.append(get<1>(tmp));
	}

	if (input_channel_parameters) {
		if (added_parameters) {
			output.append(")\n" + prefix +"{\n");
		}
		else {
			output.append("void) {\n");
		}
	}
	output.append(middle_of_output);

	for (auto v : action->get_ast()->vars) {
		auto x = Converter_RVC_Cpp::convert_vardef(v, prefix + "\t", false, replacements, const_map);
		output.append(x.first);
		output.append(x.second);
	}

	/* additional_code initializes the (internal merge channel) variables, e.g. resets their
	 * size counters. It must added before the action body. */
	output.append(additional_code);

	for (auto s : action->get_ast()->statements) {
		auto x = Converter_RVC_Cpp::convert_statement(s, prefix + "\t", replacements, const_map);
		output.append(x);
	}

	return output + end_of_output + prefix + "}\n";
}