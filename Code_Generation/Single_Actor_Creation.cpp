#include "Code_Generation.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <fstream>
#include "Scheduling/Scheduling.hpp"
#include <string>
#include <map>
#include <set>
#include "Conversion/Converter_RVC_Cpp.hpp"
#include "Action_Conversion.hpp"
#include "String_Helper.h"

static std::string class_variable_generation(
	IR::Actor_Instance* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Actor_Conversion_Data& data)
{
	std::string ret;

	for (auto it = actor->get_actor()->get_var_buffers().begin();
		it != actor->get_actor()->get_var_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		std::string tmp = Converter_RVC_Cpp::convert_expression(t, *it, data.get_symbol_map(), "\t");
		if (tmp.find_first_of(";") != tmp.find_last_of(";")) {
			// There is more than one ; in the result. This is some comprehension. Add it to the constructor.
			ret += tmp.substr(0, tmp.find_first_of(";"));
			tmp = tmp.substr(tmp.find_first_of(";") + 1);
			replace_all_substrings(tmp, "\t", "\t\t");
			data.add_constructor_code(tmp);
		}
		ret.append(tmp);
	}

	// Parameters
	std::string parameters;
	for (auto it = actor->get_actor()->get_param_buffers().begin();
		it != actor->get_actor()->get_param_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		parameters += Converter_RVC_Cpp::convert_actor_parameters(t, *it, actor->get_conversion_data().get_symbol_map(),
			constructor_parameter_name_type_map, data.get_default_parameter_map(), "\t");
	}

	ret.append("\n");
	ret.append("\t// Actor Parameters\n");
	ret.append(parameters);
	ret.append("\tstd::string actor$name;\n");

	if (!actor->get_actor()->get_in_buffers().empty()) {
		ret.append("\n");
		ret.append("\t// Input Channels\n");
		// Channels
		for (auto it = actor->get_actor()->get_in_buffers().begin();
			it != actor->get_actor()->get_in_buffers().end(); ++it)
		{
			if (unused_in_channels.find(it->buffer_name) != unused_in_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::string type = "Data_Channel<" + it->type + ">*";
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append("\t" + type + " " + it->buffer_name + ";\n");
		}
	}

	if (!actor->get_actor()->get_out_buffers().empty()) {
		ret.append("\t// Output Channels\n");
		for (auto it = actor->get_actor()->get_out_buffers().begin();
			it != actor->get_actor()->get_out_buffers().end(); ++it)
		{
			if (unused_out_channels.find(it->buffer_name) != unused_out_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::string type = "Data_Channel<" + it->type + ">*";
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append("\t" + type + " " + it->buffer_name + ";\n");
		}
	}

	ret.append("\n");

	return ret;
}

static std::string constructor_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::string class_name,
	Actor_Conversion_Data &data)
{
	std::string ret;
	std::string body;

	ret = "\t" + class_name + "(std::string _n";

	for (auto it = constructor_parameter_name_type_map.begin();
		it != constructor_parameter_name_type_map.end(); ++it)
	{
		ret.append(", ");
		ret.append(it->second + " _" + it->first);
		body.append("\t\t" + it->first + " = _" + it->first + ";\n");
		data.add_parameter(it->first);
	}
	ret.append(") {\n");
	ret.append("\t\tactor$name = _n;\n");
	ret.append(body);
	ret.append(data.get_constructor_code());
	ret.append("\t};\n");

	return ret;
}

static std::string action_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Actor_Conversion_Data& data)
{
	std::string ret;

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		// The init action is converted later and added to the public part of the class
		if ((*it)->is_init()) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}
		if (unused_actions.find((*it)->get_name()) != unused_actions.end()) {
			// action is not used, don't generate
			continue;
		}
		(*it)->get_action_buffer()->reset_buffer();
		/* Cannot use input_channel_parameters for the action because the scheduler cannot handle this
		 * right now properly. It will prefetch the tokens before checking whether sufficient output
		 * channel space is available. If this is not the case the tokens are lost.
		 * Hence, this might only work for SISO actors. Deactivate for now.
		 */
		ret += convert_action(*it, (*it)->get_action_buffer(), data,
								false, false,
								unused_in_channels, unused_out_channels, "\t");
	}

	return ret;
}

static std::string function_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	Actor_Conversion_Data& data)
{
	std::string ret;

	for (auto it = actor->get_method_buffers().begin();
		it != actor->get_method_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		if (t.str == "function") {
			ret += Converter_RVC_Cpp::convert_function(t, *it, data.get_symbol_map(), "\t");
		}
		if (t.str == "procedure") {
			ret += Converter_RVC_Cpp::convert_procedure(t, *it, data.get_symbol_map(), "\t");
		}
	}

	return ret;
}

static std::string convert_natives(IR::Actor* actor)
{
	std::string ret;

	for (auto it = actor->get_native_buffers().begin();
		it != actor->get_native_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		ret += Converter_RVC_Cpp::convert_native_declaration(t, (*it), "*",
			actor->get_conversion_data(), actor->get_conversion_data().get_symbol_map());
	}

	return ret;
}

static std::string init_action_generation(
	IR::Actor* actor,
	Actor_Conversion_Data& data)
{
	std::string ret;

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		if ((*it)->is_init()) {
			(*it)->get_action_buffer()->reset_buffer();
			ret += convert_action(*it, (*it)->get_action_buffer(), data,
				true, false, std::set<std::string>(), std::set<std::string>(), "\t");
		}
	}

	if (ret.empty()) {
		// no init function present, create an empty one
		ret.append("\tvoid init(void) {}\n");
	}

	return ret;
}

//TODO: Maybe some states are not required if actions are removed
std::string generate_FSM(
	IR::Actor* actor,
	std::set<std::string>& unused_actions)
{
	if (actor->get_fsm().empty()) {
		return std::string();
	}

	std::set<std::string> states;
	for (auto it = actor->get_fsm().begin();
		it != actor->get_fsm().end(); ++it)
	{
		states.insert(it->state);
	}

	std::string ret;
	ret.append("\t// FSM\n");
	ret.append("\tenum class FSM {\n");

	for (auto it = states.begin(); it != states.end(); ++it) {
		ret.append("\t\t" + *it + ",\n");
	}
	ret.append("\t};\n");
	ret.append("\tFSM state = FSM::" + actor->get_initial_state() + ";\n\n");

	return ret;
}

std::string Code_Generation::generate_actor_code(
	IR::Actor_Instance* instance,
	std::string class_name,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	IR::Actor* actor = instance->get_actor();

#ifdef	DEBUG_ACTOR_GENERATION
	std::cout << "Generation of actor " << actor->get_class_name() << " with class name " << class_name << std::endl;
#endif

	std::string code;
	Config* c = c->getInstance();
	std::map<std::string, std::string> constructor_parameter_name_type_map;

	Actor_Conversion_Data& d = instance->get_conversion_data();

	code.append("#pragma once\n");
	code.append("#include <iostream>\n");
	code.append("#include \"Channel.hpp\"\n");
	code.append("#include \"Actor.hpp\"\n");
	if (c->get_orcc_compat()) {
		code.append("#include \"options.h\"\n");
	}
	code.append("\n");
	code.append(d.get_declarations_code());
	code.append(convert_natives(actor));
	code.append("\n");
	code.append("class " + class_name + " : public Actor {\n");
	code.append("private:\n");
	code.append(d.get_var_code());
	code.append(class_variable_generation(instance, opt_data1, opt_data2, map_data,
		constructor_parameter_name_type_map, unused_in_channels, unused_out_channels, d));
	code.append(generate_FSM(actor, unused_actions));
	code.append(function_generation(actor, opt_data1, opt_data2, map_data, d));
	code.append(action_generation(actor, opt_data1, opt_data2, map_data, unused_actions,
		unused_in_channels, unused_out_channels, d));
	code.append("public:\n");
	code.append(constructor_generation(actor, opt_data1, opt_data2, map_data,
		constructor_parameter_name_type_map, class_name, d));

	std::map<std::string, std::string> guard_map;
	for (auto it = instance->get_actor()->get_actions().begin();
		it != instance->get_actor()->get_actions().end(); ++it)
	{
		guard_map[(*it)->get_function_name()] = (*it)->get_guard();
	}

	code.append(Scheduling::generate_local_scheduler(
		d.get_scheduling_data(), guard_map,
		actor->get_fsm(), actor->get_priorities(),
		actor->get_input_classification(),
		actor->get_output_classification(),
		"\t"
	));
	code.append(init_action_generation(actor, d));
	code.append("};");

	std::string path{ c->get_target_dir() };

	std::ofstream output_file{ path + "\\" + class_name + ".hpp" };
	if (output_file.bad()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path + "\\" + class_name + ".hpp" };
	}
	output_file << code;
	output_file.close();

	return "#include \"" + class_name + ".hpp\"\n";
}