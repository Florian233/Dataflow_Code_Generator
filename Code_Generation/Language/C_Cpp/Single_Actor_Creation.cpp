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
#include <iostream>
#include "ABI/RTOS/RTOS_Core_Sched.hpp"

static std::pair<std::string, std::string> class_variable_generation(
	IR::Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	std::string prefix,
	std::map<std::string, std::string>& replacements,
	std::string& constructorcode,
	std::map<std::string, std::string>& defaults,
	std::string name)
{
	std::string ret;
	std::string const_ret;
	Config* c = Config::getInstance();

	for (auto it = actor->get_ast()->actor->vars.begin();
		it != actor->get_ast()->actor->vars.end(); ++it)
	{
		auto tmp = Converter_RVC_Cpp::convert_vardef(*it, prefix, (c->get_target_language() == Target_Language::c), replacements,
			actor->get_const_map(), (c->get_target_ABI() == Target_ABI::rtos));
		constructorcode.append(tmp.second);

		if ((*it)->constassign) {
			const_ret.append(tmp.first);
		}
		else {
			ret.append(tmp.first);
		}

		if ((c->get_target_ABI() != Target_ABI::rtos) && (c->get_target_language() == Target_Language::c) && !(*it)->constassign) {
			replacements[(*it)->name.name] = "_g->" + (*it)->name.name;
		}
	}

	// Parameters
	std::string parameters;
	for (auto it = actor->get_ast()->actor->parameters.begin();
		it != actor->get_ast()->actor->parameters.end(); ++it)
	{
		auto tmp = Converter_RVC_Cpp::convert_actorparam(*it, prefix, actor->get_const_map());
		constructor_parameter_name_type_map[(*it)->name.name] = Converter_RVC_Cpp::convert_type(&(*it)->type,"", actor->get_const_map());
		defaults[(*it)->name.name] = tmp.second;

		std::string x = tmp.first;
		if (c->get_target_ABI() == Target_ABI::rtos) {
			x = "static " + x;
		}

		parameters.append(x);

		if ((c->get_target_ABI() != Target_ABI::rtos) && (c->get_target_language() == Target_Language::c)) {
			replacements[(*it)->name.name] = "_g->" + (*it)->name.name;
		}
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	ret.append(prefix + "// Actor Parameters\n");
	ret.append(parameters);
	if (c->get_target_ABI() == Target_ABI::rtos) {
		ret.append(prefix + "static const char* actor_name =\"" + name + "\";\n");
	}
	else if (c->get_target_language() == Target_Language::cpp) {
		ret.append(prefix + "std::string actor_name;\n");
	}
	else {
		ret.append(prefix + "char actor_name[64];\n"); //length should be sufficient.
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	if (!actor->get_ast()->actor->inports.empty()) {
		ret.append(prefix + "// Input Channels\n");
		// Channels
		for (auto it = actor->get_ast()->actor->inports.begin();
			it != actor->get_ast()->actor->inports.end(); ++it)
		{
			if (unused_in_channels.find((*it)->name.name) != unused_in_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::pair<std::string, std::string> decl;
			std::string typex = Converter_RVC_Cpp::convert_type(&(*it)->type, "", actor->get_const_map());
			ABI_CHANNEL_DECL(c, decl, (*it)->name.name, "0", typex, false, prefix);
			std::string type = decl.second + "*";
			constructor_parameter_name_type_map[(*it)->name.name] = type;
			ret.append(decl.first);
		}
	}

	if (!actor->get_ast()->actor->outports.empty()) {
		ret.append(prefix + "// Output Channels\n");
		for (auto it = actor->get_ast()->actor->outports.begin();
			it != actor->get_ast()->actor->outports.end(); ++it)
		{
			if (unused_out_channels.find((*it)->name.name) != unused_out_channels.end()) {
				//it is not used, skip
				continue;
			}
			std::pair<std::string, std::string> decl;
			std::string typex = Converter_RVC_Cpp::convert_type(&(*it)->type, "", actor->get_const_map());
			ABI_CHANNEL_DECL(c, decl, (*it)->name.name, "0", typex, false, prefix);
			std::string type = decl.second + "*";
			constructor_parameter_name_type_map[(*it)->name.name] = type;
			ret.append(decl.first);
		}
	}
	if (c->get_target_language() == Target_Language::cpp) {
		ret.append("\n");
	}
	return std::make_pair(ret, const_ret);
}

static std::string constructor_generation(
	IR::Actor_Instance* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::vector<std::string>& param_order,
	std::string class_name,
	std::string constructor_code,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& data)
{
	std::string ret;
	std::string body;
	Config* c = Config::getInstance();

	if (c->get_target_ABI() == Target_ABI::rtos) {
		ret = "void ";
	}

	ret += class_name + "(";
	if (c->get_target_ABI() != Target_ABI::rtos) {
		ret = "\t" + ret;
		ret.append("std::string _n");
	}

	for (auto it = constructor_parameter_name_type_map.begin();
		it != constructor_parameter_name_type_map.end(); ++it)
	{
		if ((c->get_target_ABI() != Target_ABI::rtos) || (it != constructor_parameter_name_type_map.begin())) {
			ret.append(", ");
		}
		ret.append(it->second + " _" + it->first);
		if (c->get_target_ABI() != Target_ABI::rtos) {
			body.append("\t");
		}
		body.append("\t" + it->first + " = _" + it->first + ";\n");
		param_order.push_back(it->first);

	}
	ret.append(")");
	rtos_register_actor_globals(ret + ";\n");
	ret.append("{\n");
	ret.append(body);
	ret.append(constructor_code);

	std::string abi_code;
	ABI_ADD_CONSTRUCTOR(c, abi_code, class_name);
	ret.append(abi_code);

	if (c->get_target_ABI() != Target_ABI::rtos) {
		ret.append("\t\tactor_name = _n;\n");
	}
	else {
		unsigned arg = 0;
		unsigned feedback_arg = 1000;
		for (auto inp : actor->get_in_edges()) {
			std::string x;
			if (inp->get_feedback()) {
				ABI_CHANNEL_REG_READ(c, x, inp->get_dst_port(), "schedcheck", std::to_string(feedback_arg));
			}
			else {
				ABI_CHANNEL_REG_READ(c, x, inp->get_dst_port(), "schedcheck", std::to_string(arg));
			}
			ret.append("\t"+ x + ";\n");
			for (auto it = data.begin(); it != data.end(); ++it) {
				for (auto s = it->second.begin(); s != it->second.end(); ++s) {
					if (s->in && (s->channel_name == inp->get_dst_port())) {
						if (inp->get_feedback()) {
							s->arg = feedback_arg;
						}
						else {
							s->arg = arg;
						}
					}
				}
			}

			if (inp->get_feedback()) {
				++feedback_arg;
			}
			else {
				++arg;
			}
		}
		for (auto outp : actor->get_out_edges()) {
			if (!outp->get_feedback()) {
				continue;
			}
			for (auto it = data.begin(); it != data.end(); ++it) {
				for (auto s = it->second.begin(); s != it->second.end(); ++s) {
					if (!s->in && (s->channel_name == outp->get_src_port())) {
						s->arg = 1000;
					}
				}
			}
		}
	}
	if (c->get_target_ABI() == Target_ABI::rtos) {
		ret.append("}\n");
	}
	else {
		ret.append("\t};\n");
	}

	return ret;
}

static std::string init_action_generation(
	IR::Actor* actor,
	std::string prefix,
	std::string class_name,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& data,
	std::map<std::string, std::string> port_type_map)
{
	std::string ret;
	Config* c = Config::getInstance();

	for (auto it = actor->get_actions().begin();
		it != actor->get_actions().end(); ++it)
	{
		if ((*it)->is_init()) {
			ret += convert_action(*it, true, std::set<std::string>(), std::set<std::string>(), prefix, replacements, data,
									class_name, port_type_map, actor->get_const_map());
		}
	}

	if (ret.empty()) {
		// no init function present, create an empty one
		if (c->get_target_language() == Target_Language::c) {
			if (c->get_target_ABI() == Target_ABI::rtos) {
				ret.append(prefix + "void " + class_name + "_initialize(void) {}\n\n");

			}
			else {
				ret.append(prefix + "void " + class_name + "_initialize(" + class_name + "_t *_g) {}\n");
			}
		}
		else {
			ret.append(prefix + "void initialize(void) {}\n");
		}
	}

	if (c->get_target_ABI() == Target_ABI::rtos) {
		rtos_register_actor_globals("void " + class_name + "_initialize(void);\n");
	}

	return ret;
}

static std::string action_generation(
	IR::Actor* actor,
	std::set<std::string>& unused_actions,
	std::set<std::string>& unused_in_channels,
	std::set<std::string>& unused_out_channels,
	std::string prefix,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data>>& data,
	std::string class_name,
	std::map<std::string, std::string> port_type_map)
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
		/* Cannot use input_channel_parameters for the action because the scheduler cannot handle this
		 * right now properly. It will prefetch the tokens before checking whether sufficient output
		 * channel space is available. If this is not the case the tokens are lost.
		 * Hence, this might only work for SISO actors. Deactivate for now.
		 */
		ret += convert_action(*it, false,
			unused_in_channels, unused_out_channels, prefix, replacements, data, class_name, port_type_map, actor->get_const_map());
	}

	return ret;
}

static std::string generate_FSM(
	IR::Actor* actor,
	std::set<std::string>& unused_actions,
	std::string class_name,
	std::string prefix)
{
	Config* c = Config::getInstance();

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

static std::string convert_import(
	IR::Actor* actor,
	std::map<std::string, std::string> r)
{
	std::vector<AST::Unit*> units;
	for (auto u : actor->get_imported_symbols()) {
		units.push_back(u->get_ast()->unit);
		for (auto uu : u->get_imported_units()) {
			units.push_back(uu->unit);
		}
	}

	Config* c = Config::getInstance();
	std::string prefix;
	if (c->get_target_language() == Target_Language::cpp) {
		prefix = "\t";
	}

	std::string functions;

	for (auto u : units) {
		
		for (auto n : u->nativefunctions) {
			functions += Converter_RVC_Cpp::convert_nativefunction(n, prefix, actor->get_const_map());
		}
		for (auto n : u->nativeprocedures) {
			functions += Converter_RVC_Cpp::convert_nativeprocedure(n, prefix, actor->get_const_map());
		}
		for (auto f : u->functions) {
			functions += Converter_RVC_Cpp::convert_function(f, prefix, r, actor->get_const_map());
		}
		for (auto p : u->procedures) {
			functions += Converter_RVC_Cpp::convert_procedure(p, prefix, r, actor->get_const_map());
		}
		for (auto v : u->vars) {
			functions += Converter_RVC_Cpp::convert_vardef(v, prefix, v->constassign, r, actor->get_const_map(), (c->get_target_ABI() == Target_ABI::rtos)).first;
		}
	}
	return functions;
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
	std::string channel_include,
	std::map<std::string, std::string>& default_constructor_params,
	std::vector<std::string>& constructor_parameter_order,
	unsigned scheduling_loop_bound)
{
	IR::Actor* actor = instance->get_actor();
	std::string header_name, source_name;

	std::map<std::string, std::string> replacements;
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> schedule_data;
	std::map<std::string, std::string> constructor_parameter_name_type_map;
	std::map<std::string, std::string> port_type_map;

	for (auto port : actor->get_ast()->actor->inports) {
		port_type_map[port->name.name] = Converter_RVC_Cpp::convert_type(&port->type, "", actor->get_const_map());
	}
	for (auto port : actor->get_ast()->actor->outports) {
		port_type_map[port->name.name] = Converter_RVC_Cpp::convert_type(&port->type, "", actor->get_const_map());
	}

#ifdef	DEBUG_ACTOR_GENERATION
	std::cout << "Generation of actor " << actor->get_class_name() << " with name " << class_name << std::endl;
#endif

	std::string header_code, source_code;
	Config* c = Config::getInstance();

	if (c->get_target_ABI() == Target_ABI::rtos) {
		replacements["true"] = "TRUE";
		replacements["false"] = "FALSE";
	}

	std::string imports = convert_import(actor, replacements);
	std::string functions;
	std::string natives;
	std::map<std::string, AST::Action*> guard_map;
	std::string constructor_code;

	for (auto n : actor->get_ast()->actor->nativefunctions) {
		natives += Converter_RVC_Cpp::convert_nativefunction(n, "", actor->get_const_map());
		actor->get_const_map()[n->name.name] = "native";
	}
	for (auto n : actor->get_ast()->actor->nativeprocedures) {
		natives += Converter_RVC_Cpp::convert_nativeprocedure(n, "", actor->get_const_map());
		actor->get_const_map()[n->name.name] = "native";
	}
	auto tmp = class_variable_generation(actor, opt_data1, opt_data2, map_data,
		constructor_parameter_name_type_map, unused_in_channels, unused_out_channels, c->get_target_ABI() == Target_ABI::rtos ? "" : "\t", replacements,
		constructor_code, default_constructor_params, class_name);

	std::string add_args = "";
	if (c->get_target_ABI() == Target_ABI::stdc) {
		add_args = c->get_globals_prefix() + class_name + "_t* _g";
	}

	for (auto f : actor->get_ast()->actor->functions) {
		functions += Converter_RVC_Cpp::convert_function(f, c->get_target_language() == Target_Language::cpp ? "\t" : "",
			replacements, actor->get_const_map(), add_args);
	}
	for (auto p : actor->get_ast()->actor->procedures) {
		functions += Converter_RVC_Cpp::convert_procedure(p, c->get_target_language() == Target_Language::cpp ? "\t" : "",
			replacements, actor->get_const_map(), add_args);
	}

	if (c->get_target_ABI() == Target_ABI::rtos) {
		source_name = class_name + ".c";

		source_code.append("#include \"actors.h\"\n");
		source_code.append(channel_include);
		source_code.append("\n");
		replace_all_substrings(tmp.second, "\t", "");
		source_code.append(tmp.second);
		source_code.append("\n");
		source_code.append(generate_FSM(actor, unused_actions, class_name, ""));
		source_code.append("\n");
		source_code.append(tmp.first);
		if (!instance->get_actor()->get_fsm().empty()) {
			source_code.append("static " + class_name + "_fsm_t state = " + actor->get_initial_state() + ";\n");
		}
		source_code.append("//#define PRINT_FIRINGS\n");
		source_code.append("\n");
		source_code.append(imports);
		source_code.append(natives);
		source_code.append(functions);
		source_code.append("\n");
		source_code.append(action_generation(actor, unused_actions,
			unused_in_channels, unused_out_channels, "", replacements, schedule_data, class_name, port_type_map));
		source_code.append("\n");

		/* Must be done after action generation otherwise the function name is not defined */
		for (auto it = instance->get_actor()->get_actions().begin();
			it != instance->get_actor()->get_actions().end(); ++it)
		{
			guard_map[(*it)->get_function_name()] = (*it)->get_ast();
		}

		// Generates data required by the local scheduler generation but code must be placed at the end
		std::string constructor = constructor_generation(instance, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, constructor_parameter_order, c->get_globals_prefix() + class_name, constructor_code, schedule_data);

		source_code.append(Scheduling::generate_local_scheduler(
			guard_map,
			actor->get_fsm(), actor->get_priorities(),
			actor->get_input_classification(),
			actor->get_output_classification(),
			"",
			c->get_globals_prefix() + class_name + "_schedule",
			"",
			schedule_data,
			replacements,
			scheduling_loop_bound,
			false,
			instance->get_source(),
			instance->get_sink()
		));
		source_code.append("\n");
		std::string i = init_action_generation(actor, "", c->get_globals_prefix() + class_name, replacements, schedule_data, port_type_map);
		source_code.append(i);
		source_code.append(constructor);
	} else if (c->get_target_language() == Target_Language::cpp) {
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
		header_code.append(natives);
		header_code.append("\n");
		header_code.append("class " + c->get_globals_prefix() + class_name + " : public Actor {\n");
		header_code.append("private:\n");
		header_code.append(imports);
		header_code.append(functions);
		header_code.append(tmp.second + tmp.first);
		header_code.append(generate_FSM(actor, unused_actions, class_name, "\t"));
		header_code.append("\n");
		header_code.append(action_generation(actor, unused_actions,
			unused_in_channels, unused_out_channels, "\t", replacements, schedule_data, class_name, port_type_map));
		header_code.append("public:\n");
		header_code.append(constructor_generation(instance, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, constructor_parameter_order, c->get_globals_prefix() + class_name, constructor_code, schedule_data));

		/* Must be done after action generation otherwise the function name is not defined */
		for (auto it = instance->get_actor()->get_actions().begin();
			it != instance->get_actor()->get_actions().end(); ++it)
		{
			/* Replacements are done by the local scheduler creation function as it also generated the prefetches. */
			guard_map[(*it)->get_function_name()] = (*it)->get_ast();
		}

		header_code.append(Scheduling::generate_local_scheduler(
			guard_map,
			actor->get_fsm(), actor->get_priorities(),
			actor->get_input_classification(),
			actor->get_output_classification(),
			"\t",
			"schedule",
			"void",
			schedule_data,
			replacements,
			scheduling_loop_bound
		));
		header_code.append(init_action_generation(actor, "\t", class_name, replacements, schedule_data, port_type_map));
		header_code.append("};");
	}
	else {
		header_name = class_name + ".h";
		source_name = class_name + ".c";

		//Header : Guard + Struct + Schedule and Init function
		std::string include_guard = class_name;
		to_upper_case(include_guard);
		header_code = "#ifndef " + include_guard + "_H\n";
		header_code.append("#define " + include_guard + "_H\n\n");
		header_code.append(channel_include);
		header_code.append("\n");
		header_code.append(generate_FSM(actor, unused_actions, class_name, ""));
		header_code.append("\n");
		header_code.append("typedef struct " + c->get_globals_prefix() + class_name + " {\n");
		header_code.append(tmp.first);
		if (!instance->get_actor()->get_fsm().empty()) {
			header_code.append("\t" + class_name + "_fsm_t state;\n");
		}
		header_code.append("} " + c->get_globals_prefix() + class_name + "_t;\n\n");
		/* Simply call this to fill the parameter order map */
		constructor_generation(instance, opt_data1, opt_data2, map_data,
			constructor_parameter_name_type_map, constructor_parameter_order, c->get_globals_prefix() + class_name, constructor_code, schedule_data);
		header_code.append("void " + c->get_globals_prefix() + class_name + "_schedule(" + c->get_globals_prefix() + class_name + "_t *_g);\n");
		header_code.append("\n");
		header_code.append("void " + c->get_globals_prefix() + class_name + "_initialize(" + c->get_globals_prefix() + class_name + "_t *_g);\n");
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
		source_code.append(imports);
		replace_all_substrings(tmp.second, "\t", "");
		source_code.append(tmp.second);
		source_code.append("\n");
		source_code.append(natives);
		source_code.append(functions);
		source_code.append("\n");
		source_code.append(action_generation(actor, unused_actions,
			unused_in_channels, unused_out_channels, "", replacements, schedule_data, c->get_globals_prefix() + class_name, port_type_map));
		source_code.append("\n");

		/* Must be done after action generation otherwise the function name is not defined */
		for (auto it = instance->get_actor()->get_actions().begin();
			it != instance->get_actor()->get_actions().end(); ++it)
		{
			/* Replacements are done by the local scheduler creation function as it also generated the prefetches. */
			guard_map[(*it)->get_function_name()] = (*it)->get_ast();
		}

		source_code.append(Scheduling::generate_local_scheduler(
			guard_map,
			actor->get_fsm(), actor->get_priorities(),
			actor->get_input_classification(),
			actor->get_output_classification(),
			"",
			c->get_globals_prefix() + class_name + "_schedule",
			c->get_globals_prefix() + class_name+"_t* _g",
			schedule_data,
			replacements,
			scheduling_loop_bound
		));
		source_code.append("\n");
		std::string i = init_action_generation(actor, "", c->get_globals_prefix() + class_name, replacements, schedule_data, port_type_map);
		if (!actor->get_fsm().empty()) {
			//Patch init state initialization into init action
			i.pop_back();
			i.pop_back();
			i.append(constructor_code);
			i.append("\t_g->state = " + actor->get_initial_state() + ";\n}\n");
		}
		source_code.append(i);
	}

	if (!header_name.empty() && !header_code.empty()) {
		std::filesystem::path path_header{ c->get_target_dir() };
		path_header /= header_name;
		std::ofstream output_file_header{ path_header };
		if (output_file_header.fail()) {
			throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_header.string() };
		}
		output_file_header << header_code;
		output_file_header.close();
	}

	if (!source_name.empty() && !source_code.empty()) {
		std::filesystem::path path_source{ c->get_target_dir() };
		path_source /= source_name;
		std::ofstream output_file_source{ path_source };
		if (output_file_source.fail()) {
			throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path_source.string() };
		}
		output_file_source << source_code;
		output_file_source.close();
	}

	return std::make_pair(header_name, source_name);
}