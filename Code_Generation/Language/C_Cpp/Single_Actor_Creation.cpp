#include "Generate_C_Cpp.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <fstream>
#include "Scheduling.hpp"
#include <string>
#include <map>
#include <set>
#include "Converter_RVC_Cpp.hpp"
#include "Action_Conversion.hpp"
#include "String_Helper.h"
#include <filesystem>
#include "ABI/abi.hpp"

static std::pair<std::string, std::string> class_variable_generation(
	IR::Actor_Instance* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;
	std::string const_ret;
	Config* c = c->getInstance();

	for (auto it = actor->get_actor()->get_var_buffers().begin();
		it != actor->get_actor()->get_var_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		std::string symbol_name;
		bool const_def = false;
		std::string tmp = Converter_RVC_Cpp::convert_expression(t, *it, data.get_symbol_map(), data.get_symbol_type_map(), symbol_name, prefix);
		if (tmp.find_first_of(";") != tmp.find_last_of(";")) {
			// There is more than one ; in the result. This is some comprehension. Add it to the constructor.
			if (tmp.find("static") != tmp.npos) {
				// It is not really const, but is static because we only tag it as not const
				// due to the initialization that has to be done by the init, otherwise it is const.
				// Hence, it is static, so no need to add it to actor type!
				const_ret.append(tmp.substr(0, tmp.find_first_of(";")));
				const_def = true;
			}
			else {
				ret += tmp.substr(0, tmp.find_first_of(";"));
			}
			tmp = tmp.substr(tmp.find_first_of(";") + 1);
			replace_all_substrings(tmp, "\t", "\t\t");
			data.add_constructor_code(tmp);
		}
		else {
			if (tmp.find("const") != tmp.npos) {
				const_ret.append(tmp);
				const_def = true;
			}
			else {
				ret.append(tmp);
			}
		}
		data.add_class_variable(symbol_name);
		if ((c->get_target_language() == Target_Language::c) && !const_def) {
			data.add_replacement(symbol_name, "_g->" + symbol_name);
		}
	}

	// Parameters
	std::string parameters;
	for (auto it = actor->get_actor()->get_param_buffers().begin();
		it != actor->get_actor()->get_param_buffers().end(); ++it)
	{
		it->reset_buffer();
		Token t = it->get_next_Token();
		parameters += Converter_RVC_Cpp::convert_actor_parameters(t, *it, actor->get_conversion_data().get_symbol_map(),
			constructor_parameter_name_type_map, data.get_default_parameter_map(), prefix);
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	ret.append(prefix + "// Actor Parameters\n");
	ret.append(parameters);
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append(prefix + "std::string actor_name;\n");
	}
	else {
		ret.append(prefix + "char actor_name[64];\n"); //length should be sufficient.
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	if (!actor->get_actor()->get_in_buffers().empty()) {
		ret.append(prefix + "// Input Channels\n");
		// Channels
		for (auto it = actor->get_actor()->get_in_buffers().begin();
			it != actor->get_actor()->get_in_buffers().end(); ++it)
		{
			if (unused_in_channels.find(it->buffer_name) != unused_in_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::pair<std::string, std::string> decl;
			ABI_CHANNEL_DECL(c, decl, it->buffer_name, "0", it->type, false, prefix);
			std::string type = decl.second + "*";
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append(decl.first);
		}
	}

	if (!actor->get_actor()->get_out_buffers().empty()) {
		ret.append(prefix + "// Output Channels\n");
		for (auto it = actor->get_actor()->get_out_buffers().begin();
			it != actor->get_actor()->get_out_buffers().end(); ++it)
		{
			if (unused_out_channels.find(it->buffer_name) != unused_out_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::pair<std::string, std::string> decl;
			ABI_CHANNEL_DECL(c, decl, it->buffer_name, "0", it->type, false, prefix);
			std::string type = decl.second + "*";
			constructor_parameter_name_type_map[it->buffer_name] = type;
			ret.append(decl.first);
		}
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	return std::make_pair(ret, const_ret);
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
	ret.append("\t\tactor_name = _n;\n");
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
	Actor_Conversion_Data& data,
	std::string prefix)
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
								unused_in_channels, unused_out_channels, prefix);
	}

	return ret;
}

static std::string function_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;

	for (auto it = actor->get_method_buffers().begin();
		it != actor->get_method_buffers().end(); ++it)
	{
		std::map<std::string, std::string> local_type_map{ data.get_symbol_type_map() };
		it->reset_buffer();
		Token t = it->get_next_Token();
		if (t.str == "function") {
			ret += Converter_RVC_Cpp::convert_function(t, *it, data.get_symbol_map(), local_type_map, prefix);
		}
		if (t.str == "procedure") {
			ret += Converter_RVC_Cpp::convert_procedure(t, *it, data.get_symbol_map(), local_type_map, prefix);
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
	Actor_Conversion_Data& data,
	std::string prefix)
{
	std::string ret;
	Config* c = c->getInstance();

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		if ((*it)->is_init()) {
			(*it)->get_action_buffer()->reset_buffer();
			ret += convert_action(*it, (*it)->get_action_buffer(), data,
				true, false, std::set<std::string>(), std::set<std::string>(), prefix);
		}
	}

	if (ret.empty()) {
		// no init function present, create an empty one
		if (c->get_target_language() == Target_Language::c) {
			ret.append(prefix + "void " + data.get_class_name() + "_initialize(" + data.get_class_name() + "_t *_g) {}\n");
		}
		else {
			ret.append(prefix + "void initialize(void) {}\n");
		}
	}

	return ret;
}

//TODO: Maybe some states are not required if actions are removed
static std::string generate_FSM(
	IR::Actor* actor,
	std::set<std::string>& unused_actions,
	std::string class_name,
	std::string prefix)
{
	Config* c = c->getInstance();

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
	ret.append(prefix + "// FSM\n");
	if (c->get_target_language() == Target_Language::c) {
		ret.append(prefix + "typedef enum " + class_name + "_fsm {\n");
	}
	else if (c->get_target_language() == Target_Language::cpp) {
		ret.append(prefix + "enum class FSM {\n");
	}

	for (auto it = states.begin(); it != states.end(); ++it) {
		ret.append(prefix + "\t" + *it + ",\n");
	}
	if (c->get_target_language() == Target_Language::c) {
		ret.append(prefix + "} " + class_name + "_fsm_t;\n");
	}
	else if (c->get_target_language() == Target_Language::cpp) {
		ret.append(prefix + "};\n");
		ret.append(prefix + "FSM state = FSM::" + actor->get_initial_state() + ";\n\n");
	}
	return ret;
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::generate_actor_code(
	IR::Actor_Instance* instance,
	std::string class_name,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::string channel_include)
{
	IR::Actor* actor = instance->get_actor();
	std::string header_name, source_name;

#ifdef	DEBUG_ACTOR_GENERATION
	std::cout << "Generation of actor " << actor->get_class_name() << " with name " << class_name << std::endl;
#endif

	std::string header_code, source_code;
	Config* c = c->getInstance();
	std::map<std::string, std::string> constructor_parameter_name_type_map;

	Actor_Conversion_Data& d = instance->get_conversion_data();

	std::map<std::string, std::string> guard_map;

	if (c->get_target_language() == Target_Language::cpp) {
		header_name = class_name + ".hpp";
		header_code.append("#pragma once\n");
		header_code.append("#include <iostream>\n");
		header_code.append("#include <string>\n");
		header_code.append(channel_include);
		header_code.append("#include \"Actor.hpp\"\n");
		if (c->get_orcc_compat()) {
			header_code.append("#include \"options.h\"\n");
		}
		header_code.append("\n");
		header_code.append(d.get_declarations_code());
		header_code.append(convert_natives(actor));
		header_code.append("\n");
		header_code.append("class " + class_name + " : public Actor {\n");
		header_code.append("private:\n");
		header_code.append(d.get_var_code());
		auto tmp = class_variable_generation(instance, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, unused_in_channels, unused_out_channels, d, "\t");
		header_code.append(tmp.second + tmp.first);
		header_code.append(generate_FSM(actor, unused_actions, class_name, "\t"));
		header_code.append(function_generation(actor, opt_data1, opt_data2, map_data, d, "\t"));
		header_code.append(action_generation(actor, opt_data1, opt_data2, map_data, unused_actions,
			unused_in_channels, unused_out_channels, d, "\t"));
		header_code.append("public:\n");
		header_code.append(constructor_generation(actor, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, class_name, d));
		for (auto it = instance->get_actor()->get_actions().begin();
			it != instance->get_actor()->get_actions().end(); ++it)
		{
			guard_map[(*it)->get_function_name()] = (*it)->get_guard();
		}
		header_code.append(Scheduling::generate_local_scheduler(
			d, guard_map,
			actor->get_fsm(), actor->get_priorities(),
			actor->get_input_classification(),
			actor->get_output_classification(),
			"\t",
			"schedule",
			"void"
		));
		header_code.append(init_action_generation(actor, d, "\t"));
		header_code.append("};");
	}
	else {
		to_lower_case(class_name);
		//override class name to have it all lower case!
		instance->get_actor()->get_conversion_data().set_class_name(class_name);
		header_name = class_name + ".h";
		source_name = class_name + ".c";

		//Header : Guard + Struct + Schedule and Init function
		std::string include_guard = class_name;
		to_upper_case(include_guard);
		header_code = "#ifndef " + include_guard + "_H\n";
		header_code.append("#define " + include_guard + "_H\n\n");
		header_code.append(channel_include);
		header_code.append("\n");
		if (!instance->get_actor()->get_fsm().empty()) {
			header_code.append("typedef enum " + class_name + "_fsm " + class_name + "_fsm_t;\n");
			header_code.append("\n");
		}
		header_code.append("typedef struct " + class_name + " {\n");
		auto tmp = class_variable_generation(instance, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, unused_in_channels, unused_out_channels, d, "\t");
		header_code.append(tmp.first);
		if (!instance->get_actor()->get_fsm().empty()) {
			header_code.append("\t" + class_name + "_fsm_t state;\n");
		}
		header_code.append("} " + class_name + "_t;\n\n");
		/* Simply call this to fill the parameter order map */
		constructor_generation(actor, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, class_name, d);
		header_code.append("void " + class_name + "_schedule(" + class_name + "_t *_g);\n");
		header_code.append("\n");
		header_code.append("void " + class_name + "_initialize(" + class_name + "_t *_g);\n");
		header_code.append("\n");
		header_code.append("#endif");
		source_code = "#include\"" + class_name + ".h\"\n";
		if (c->get_orcc_compat()) {
			source_code.append("#include \"options.h\"\n");
		}
		source_code.append("#include <stdio.h>\n");
		source_code.append("\n");
		source_code.append("//#define PRINT_FIRINGS\n");
		source_code.append("\n");
		std::string q = d.get_declarations_code();
		q.append(d.get_var_code());
		replace_all_substrings(q, "\t", "");
		source_code.append(q);
		source_code.append("\n");
		source_code.append(generate_FSM(actor, unused_actions, class_name, ""));
		source_code.append("\n");
		source_code.append(convert_natives(actor));
		source_code.append("\n");
		replace_all_substrings(tmp.second, "\t", "");
		source_code.append(tmp.second);
		source_code.append("\n");
		source_code.append(function_generation(actor, opt_data1, opt_data2, map_data, d, ""));
		source_code.append("\n");
		source_code.append(action_generation(actor, opt_data1, opt_data2, map_data, unused_actions,
			unused_in_channels, unused_out_channels, d, ""));
		source_code.append("\n");
		for (auto it = instance->get_actor()->get_actions().begin();
			it != instance->get_actor()->get_actions().end(); ++it)
		{
			guard_map[(*it)->get_function_name()] = (*it)->get_guard();
		}
		source_code.append(Scheduling::generate_local_scheduler(
			d, guard_map,
			actor->get_fsm(), actor->get_priorities(),
			actor->get_input_classification(),
			actor->get_output_classification(),
			"",
			class_name + "_schedule",
			class_name+"_t* _g"
		));
		source_code.append("\n");
		std::string i = init_action_generation(actor, d, "");
		if (!actor->get_fsm().empty()) {
			//Patch init state initialization into init action
			i.pop_back();
			i.pop_back();
			i.append(d.get_constructor_code());
			i.append("\t_g->state = " + actor->get_initial_state() + ";\n}\n");
		}
		source_code.append(i);
	}

	std::filesystem::path path_header{ c->get_target_dir() };
	path_header /= header_name;
	std::ofstream output_file_header{ path_header };
	if (output_file_header.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_header.string() };
	}
	output_file_header << header_code;
	output_file_header.close();

	if (!source_name.empty() && !source_code.empty()) {
		std::filesystem::path path_source{ c->get_target_dir() };
		path_source /= source_name;
		std::ofstream output_file_source{ path_source };
		if (output_file_source.fail()) {
			throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_header.string() };
		}
		output_file_source << source_code;
		output_file_source.close();
	}

	return std::make_pair(header_name, source_name);
}