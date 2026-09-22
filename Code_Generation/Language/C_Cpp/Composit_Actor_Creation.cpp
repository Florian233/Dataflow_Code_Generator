#include "Generate_C_Cpp.hpp"
#include "Config/config.h"
#include <fstream>
#include <string>
#include <map>
#include <set>
#include "Converter_RVC_Cpp.hpp"
#include "String_Helper.h"
#include <filesystem>
#include "ABI/abi.hpp"
#include <iostream>
#include "Action_Conversion.hpp"
#include "Scheduling.hpp"
#include "ABI/RTOS/RTOS_Core_Sched.hpp"

static std::pair<std::string, std::string> class_variable_generation(
	IR::Composit_Actor* actor,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
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
		constructor_parameter_name_type_map[(*it)->name.name] = Converter_RVC_Cpp::convert_type(&(*it)->type, "", actor->get_const_map());
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
	IR::Composit_Actor* actor,
	std::map<std::string, std::string>& constructor_parameter_name_type_map,
	std::vector<std::string>& param_order,
	std::string class_name,
	std::string constructor_code,
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>>& sched_data)
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
		body.append("\t\t" + it->first + " = _" + it->first + ";\n");
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
			if (inp->is_deleted()) {
				continue;
			}
			std::string x;
			if (inp->get_feedback()) {
				ABI_CHANNEL_REG_READ(c, x, inp->get_dst_port(), "schedcheck", std::to_string(feedback_arg));
			}
			else {
				ABI_CHANNEL_REG_READ(c, x, inp->get_dst_port(), "schedcheck", std::to_string(arg));
			}
			ret.append("\t" + x + ";\n");

			for (auto it = sched_data.begin(); it != sched_data.end(); ++it) {
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
			for (auto it = sched_data.begin(); it != sched_data.end(); ++it) {
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
	IR::Composit_Actor* actor,
	std::string prefix,
	std::string class_name,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::string> port_type_map,
	std::string code)
{
	std::string ret;
	Config* c = Config::getInstance();

	std::string name;
	if (c->get_target_language() == Target_Language::c) {
		name = class_name + "_initialize";
	}
	else {
		name = "initialize";
	}

	if (actor->get_ast()->actor->init == nullptr) {
		// no init function present, create an empty one
		if (c->get_target_language() == Target_Language::c) {
			if (c->get_target_ABI() == Target_ABI::rtos) {
				ret.append(prefix + "void " + class_name + "_initialize(void) {");
			}
			else {
				ret.append(prefix + "void " + class_name + "_initialize(" + class_name + "_t *_g) {");
			}
			if (!code.empty()) {
				ret.append("\n" + code + prefix);
			}
			ret.append("}\n");
		}
		else {
			ret.append(prefix + "void initialize(void) {");
			if (!code.empty()) {
				ret.append("\n" + code + prefix);
			}
			ret.append("}\n");
		}
	}
	else {
		IR::Action a{ name, actor->get_ast()->actor->init, true };
		std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> dummy;
		ret = convert_action(&a, false, std::set<std::string>(), std::set<std::string>(), prefix, replacements,
			dummy, class_name, port_type_map, actor->get_const_map(), code);
	}

	if (c->get_target_ABI() == Target_ABI::rtos) {
		rtos_register_actor_globals("void " + class_name + "_initialize(void);\n");
	}

	return ret;
}

static std::string action_generation(
	IR::Composit_Actor* actor,
	std::string prefix,
	std::map<std::string, std::string> replacements,
	std::string class_name,
	std::map<std::string, std::string> port_type_map,
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>>& sched_data,
	std::map<std::string, AST::Action*>& action_guard)
{
	std::string ret;

	for (auto action : actor->get_ast()->actor->actions) {
		IR::Action a{ action->name.name, action, false };
		action_guard[action->name.name] = action;
		ret += convert_action(&a, false, std::set<std::string>(), std::set<std::string>(), prefix, replacements,
			sched_data, class_name, port_type_map, actor->get_const_map());

		for (auto in : actor->get_in_edges()) {
			Scheduling::Channel_Schedule_Data d;
			d.channel_name = in->get_dst_port();
			d.arg = 0;
			d.elements = 1;
			d.in = true;
			sched_data[action->name.name].push_back(d);
		}
		for (auto out : actor->get_out_edges()) {
			Scheduling::Channel_Schedule_Data d;
			d.channel_name = out->get_src_port();
			d.arg = 0;
			d.elements = 1;
			d.in = false;
			sched_data[action->name.name].push_back(d);
		}
	}

	return ret;
}

static std::string convert_import(
	IR::Composit_Actor* actor,
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
			functions += Converter_RVC_Cpp::convert_vardef(v, prefix, false, r, actor->get_const_map(), (c->get_target_ABI() == Target_ABI::rtos)).first;
		}
	}
	return functions;
}

static std::string generate_FSMs(
	AST::Actor* ast,
	std::string prefix,
	std::map<std::string, std::string>& replacements)
{
	Config* c = Config::getInstance();
	std::string ret;

	if (ast->fsm_enums.empty()) {
		return "";
	}
	ret.append(prefix + "// FSM\n");
	for (auto f : ast->fsm_enums) {
		if (c->get_target_language() == Target_Language::c) {
			ret.append(prefix + "typedef enum " + f->name + "_s {\n");
		}
		else if (c->get_target_language() == Target_Language::cpp) {
			ret.append(prefix + "enum class " + f->name + " {\n");
		}

		for (auto it = f->states.begin(); it != f->states.end(); ++it) {
			if (c->get_target_language() == Target_Language::c) {
				std::string name = f->name + "_" + *it;
				replacements[*it] = name;
				ret.append(prefix + "\t" + name + ",\n");
			}
			else {
				ret.append(prefix + "\t" + *it + ",\n");
			}
		}
		if (c->get_target_language() == Target_Language::c) {
			ret.append(prefix + "} " + f->name + "_t;\n");
			replacements[f->name] = f->name + "_t";
		}
		else if (c->get_target_language() == Target_Language::cpp) {
			ret.append(prefix + "};\n");
		}
	}
	return ret;
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::generate_composit_actor_code(
	IR::Composit_Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::string channel_include,
	std::map<std::string, std::string>& default_constructor_params,
	std::vector<std::string>& constructor_parameter_order,
	unsigned scheduling_loop_bound)
{
	std::string header_name, source_name;

	std::map<std::string, std::string> replacements;
	std::map<std::string, std::string> constructor_parameter_name_type_map;
	std::map<std::string, std::string> port_type_map;

	for (auto port : actor->get_ast()->actor->inports) {
		port_type_map[port->name.name] = Converter_RVC_Cpp::convert_type(&port->type, "", actor->get_const_map());
	}
	for (auto port : actor->get_ast()->actor->outports) {
		port_type_map[port->name.name] = Converter_RVC_Cpp::convert_type(&port->type, "", actor->get_const_map());
	}

#ifdef	DEBUG_ACTOR_GENERATION
	std::cout << "Generation of composit actor " << actor->get_name() << std::endl;
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
	std::string constructor_code;
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data>> sched_data;
	std::map<std::string, AST::Action*> action_guard;
	std::vector<IR::FSM_Entry> fsm;
	std::vector<IR::Priority_Entry> priorities;
	IR::Priority_Entry p;
	p.action_high = "init_action";
	p.action_low = "do_action";
	priorities.push_back(p);
	p.action_high = p.action_low;
	p.action_low = "done_action";
	priorities.push_back(p);

	std::string fsm_code = generate_FSMs(actor->get_ast()->actor, c->get_target_language() == Target_Language::cpp ? "\t" : "", replacements);

	for (auto n : actor->get_ast()->actor->nativefunctions) {
		natives += Converter_RVC_Cpp::convert_nativefunction(n, "", actor->get_const_map());
		actor->get_const_map()[n->name.name] = "native";
	}
	for (auto n : actor->get_ast()->actor->nativeprocedures) {
		natives += Converter_RVC_Cpp::convert_nativeprocedure(n, "", actor->get_const_map());
		actor->get_const_map()[n->name.name] = "native";
	}
	auto tmp = class_variable_generation(actor,
		constructor_parameter_name_type_map, c->get_target_ABI() == Target_ABI::rtos ? "" : "\t", replacements, constructor_code,
		default_constructor_params, actor->get_name());

	std::string add_args = "";
	if (c->get_target_ABI() == Target_ABI::stdc) {
		add_args = c->get_globals_prefix() + actor->get_class() + "_t* _g";
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
		source_name = actor->get_class() + ".c";

		source_code.append("#include \"actors.h\"\n");
		source_code.append(channel_include);
		source_code.append("\n");
		replace_all_substrings(tmp.second, "\t", "");
		source_code.append(tmp.second);
		source_code.append("\n");
		source_code.append(fsm_code);
		source_code.append("\n");
		source_code.append(tmp.first);
		source_code.append("//#define PRINT_FIRINGS\n");
		source_code.append("\n");
		source_code.append(imports);
		source_code.append(natives);
		source_code.append(functions);
		source_code.append("\n");
		source_code.append(action_generation(actor, "", replacements, actor->get_class(), port_type_map, sched_data, action_guard));
		source_code.append("\n");

		// Generates data required by the local scheduler generation but code must be placed at the end
		std::string constructor = constructor_generation(actor, constructor_parameter_name_type_map,
			constructor_parameter_order, c->get_globals_prefix() + actor->get_class(), constructor_code, sched_data);

		unsigned non_feedback_inputs = 0;
		unsigned non_feedback_outputs = 0;
		for (auto in : actor->get_in_edges()) {
			if (!in->get_feedback()) {
				++non_feedback_inputs;
			}
		}
		for (auto out : actor->get_out_edges()) {
			if (!out->get_feedback()) {
				++non_feedback_outputs;
			}
		}

		source_code.append(Scheduling::generate_local_scheduler(
			action_guard,
			fsm,
			priorities,
			actor->get_input_classification(),
			actor->get_output_classification(),
			"",
			c->get_globals_prefix() + actor->get_class() + "_schedule",
			"",
			sched_data,
			replacements,
			scheduling_loop_bound,
			true,
			actor->get_source() && (non_feedback_inputs == 0),
			actor->get_sink() && (non_feedback_outputs == 0)
		));
		source_code.append("\n");
		source_code.append(init_action_generation(actor, "", c->get_globals_prefix() + actor->get_class(), replacements, port_type_map, constructor_code));
		source_code.append(constructor);
	}
	else if (c->get_target_language() == Target_Language::cpp) {
		header_name = actor->get_class() + ".hpp";
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
		header_code.append("class " + actor->get_class() + " : public Actor {\n");
		header_code.append("private:\n");
		header_code.append(imports);
		header_code.append(functions);
		header_code.append(tmp.second + tmp.first);
		header_code.append(fsm_code);
		header_code.append("\n");
		header_code.append(action_generation(actor, "\t", replacements, actor->get_class(), port_type_map, sched_data, action_guard));
		header_code.append("public:\n");
		header_code.append(constructor_generation(actor,
			constructor_parameter_name_type_map, constructor_parameter_order, actor->get_class(), constructor_code, sched_data));

		header_code.append(Scheduling::generate_local_scheduler(
			action_guard,
			fsm,
			priorities,
			actor->get_input_classification(),
			actor->get_output_classification(),
			"\t",
			"schedule",
			"void",
			sched_data,
			replacements,
			scheduling_loop_bound,
			true
		));
		header_code.append(init_action_generation(actor, "\t", actor->get_class(), replacements, port_type_map, ""));
		header_code.append("};");
	}
	else {
		header_name = actor->get_class() + ".h";
		source_name = actor->get_class() + ".c";

		//Header : Guard + Struct + Schedule and Init function
		std::string include_guard = actor->get_class();
		to_upper_case(include_guard);
		header_code = "#ifndef " + include_guard + "_H\n";
		header_code.append("#define " + include_guard + "_H\n\n");
		header_code.append(channel_include);
		header_code.append("\n");
		header_code.append(fsm_code);
		header_code.append("\n");
		header_code.append("typedef struct " + c->get_globals_prefix() + actor->get_class() + " {\n");
		header_code.append(tmp.first);
		header_code.append("} " + c->get_globals_prefix() + actor->get_class() + "_t;\n\n");
		/* Simply call this to fill the parameter order map */
		constructor_generation(actor, constructor_parameter_name_type_map, constructor_parameter_order, c->get_globals_prefix() + actor->get_class(), constructor_code, sched_data);
		header_code.append("void " + c->get_globals_prefix() + actor->get_class() + "_schedule(" + c->get_globals_prefix() + actor->get_class() + "_t *_g);\n");
		header_code.append("\n");
		header_code.append("void " + c->get_globals_prefix() + actor->get_class() + "_initialize(" + c->get_globals_prefix() + actor->get_class() + "_t *_g);\n");
		header_code.append("\n");
		header_code.append("#endif");
		source_code = "#include\"" + actor->get_class() + ".h\"\n";
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
		source_code.append(action_generation(actor, "", replacements, actor->get_class(), port_type_map, sched_data, action_guard));
		source_code.append("\n");

		source_code.append(Scheduling::generate_local_scheduler(
			action_guard,
			fsm,
			priorities,
			actor->get_input_classification(),
			actor->get_output_classification(),
			"",
			c->get_globals_prefix() + actor->get_class() + "_schedule",
			c->get_globals_prefix() + actor->get_class() + "_t* _g",
			sched_data,
			replacements,
			scheduling_loop_bound,
			true
		));
		source_code.append("\n");
		source_code.append(init_action_generation(actor, "", c->get_globals_prefix() + actor->get_class(), replacements, port_type_map, constructor_code));
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