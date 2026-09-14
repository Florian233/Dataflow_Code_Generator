#include "Generate_C_Cpp.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include "Scheduling.hpp"
#include <string>
#include <fstream>
#include <map>
#include <filesystem>
#include "ABI/abi.hpp"
#include <iostream>
#include "Converter_RVC_Cpp.hpp"
#include "ABI/RTOS/RTOS_Core_Sched.hpp"

/* Map each channel to the concrete channel implementation that is used for this channel. */
static std::map<std::string, std::string> channel_impl_map;
/* Map each channel to the type of the tokens it carries. */
static std::map<std::string, std::string> channel_type_map;
/* Map each channel to its size (number of tokens it can carry). */
static std::map<std::string, std::string> channel_size_map;
/* Map actor_instance_name_port_name to the name of the generated channel. */
static std::map<std::string, std::string> actorport_channel_map;
static std::map<std::string, IR::Actor_Instance*> actorname_instance_map; //Not for composit actors, they carry their parameters inside!
static std::vector<std::string> global_scheduling_routines;

static std::string generate_actor_constructor_parameters(
	std::string name,
	bool static_alloc,
	std::vector<std::string> param_order,
	std::map<std::string, std::string> default_params)
{
	std::string result;
	Config* c = Config::getInstance();

	if (c->get_target_language() == Target_Language::c) {
		result.append("\n\t\t.actor_name = ");
	}
	result.append("\"" + name + "\"");
	for (auto param_it = param_order.begin();
		param_it != param_order.end(); ++param_it)
	{
		result.append(", ");
		if (c->get_target_language() == Target_Language::c) {
			result.append("\n\t\t." + *param_it + " = ");
		}
		if (actorport_channel_map.contains(name + "_" + *param_it)) {
			if (static_alloc) {
				result.append("&");
			}
			result.append(actorport_channel_map[name + "_" + *param_it]);
		}
		else if (actorname_instance_map.contains(name)) {
			//only true for non-merged actor instances
			IR::Actor_Instance* instance = actorname_instance_map[name];
			if (instance->get_parameters().contains(*param_it)) {
				result.append(actorname_instance_map[name]->get_parameters()[*param_it]);
			}
			else if (default_params.contains(*param_it)) {
				result.append(default_params[*param_it]);
			}
			else {
				if (instance->is_port(*param_it)) {
					if (c->get_target_language() == Target_Language::c) {
						result.append("NULL");
					}
					else {
						result.append("nullptr");
					}
				}
				else {
					// No Parameter value in the network, no default parameter = bug
					throw Code_Generation::Code_Generation_Exception{ "No Parameter value given for " + name + " parameter: " + *param_it };
				}
			}
		}
		else {
			//something is wrong here, this cannot happen
			std::cout << "ERROR: Parameter insertion for actor constructor failed!" << std::endl;
			exit(6);
		}

	}
	
	return result;
}

static std::string determine_port_type(
	IR::Actor_Instance_Base* inst,
	std::string port)
{
	std::string res;
	if ((inst == nullptr) || (inst->get_ast() == nullptr) || (inst->get_ast()->actor == nullptr)) {
		return res;
	}
	for (auto in : inst->get_ast()->actor->inports) {
		if (in->name.name == port) {
			res = Converter_RVC_Cpp::convert_type(&in->type, "", inst->get_const_map());
		}
	}
	for (auto out : inst->get_ast()->actor->outports) {
		if (out->name.name == port) {
			res = Converter_RVC_Cpp::convert_type(&out->type, "", inst->get_const_map());
		}
	}
	return res;
}

static std::string generate_channels(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	std::string result;
	Config* c = Config::getInstance();
	for (auto it = dpn->get_edges().begin();
		it != dpn->get_edges().end(); ++it)
	{
		IR::Actor_Instance_Base* source = (*it)->get_source();
		IR::Actor_Instance_Base* sink = (*it)->get_sink();
		if ((*it)->is_deleted()) {
			continue;
		}

		if ((source == nullptr) || (sink == nullptr)) {
			throw Converter_Exception{
				"Channel generation detected an edge with an unconnected port: "
				+ (*it)->get_src_id() + "." + (*it)->get_src_port() + " -> "
				+ (*it)->get_dst_id() + "." + (*it)->get_dst_port() };
		}

		std::string name;

		name = (*it)->get_src_id() + "_" + (*it)->get_src_port() + "_" + (*it)->get_dst_id() + "_" + (*it)->get_dst_port();

		if (channel_impl_map.contains(name)) {
			//just a sanity check, this cannot happen I think
			std::cout << "ERROR: Determined channel name that is already in use: " << name << std::endl;
			exit(5);
		}

		std::string typeSource = determine_port_type(source, (*it)->get_src_port());
		std::string typeSink = determine_port_type(sink, (*it)->get_dst_port());

		if (typeSource != typeSink) {
#if 0
			throw Code_Generation::Code_Generation_Exception{
				"Types of " + (*it)->get_source()->get_name() + "." + (*it)->get_src_port()
				+ " and " + (*it)->get_sink()->get_name() + "." + (*it)->get_dst_port() + " don't match."};
#else
			std::cout << "WARNING: Types of " + (*it)->get_source()->get_name() + "." + (*it)->get_src_port()
				+ " and " + (*it)->get_sink()->get_name() + "." + (*it)->get_dst_port() + " don't match.\n";
#endif
		}
		actorport_channel_map[(*it)->get_source()->get_name() + "_" + (*it)->get_src_port()] = name;
		actorport_channel_map[(*it)->get_sink()->get_name() + "_" + (*it)->get_dst_port()] = name;

		channel_type_map[name] = typeSource;
		std::string chan_sz;
		if ((*it)->get_specified_size() == c->get_FIFO_size()) {
			 chan_sz = "CHANNEL_SIZE";
		}
		else {
			chan_sz = std::to_string((*it)->get_specified_size());
		}
		channel_size_map[name] = chan_sz;

		std::pair<std::string, std::string> decl;
		ABI_CHANNEL_DECL(c, decl, name, chan_sz, typeSource, c->get_static_alloc(), "");

		channel_impl_map[name] = decl.second;
		result.append(decl.first);
	}

	return result;
}

static std::string generate_actor_instances(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::map<std::string, std::string>> default_param_maps,
	std::map<std::string, std::vector<std::string>> param_order_map)
{
	std::string result;
	Config* c = Config::getInstance();

	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}

		std::string t = c->get_globals_prefix() + (*it)->get_identifier();
		if (c->get_target_language() == Target_Language::c) {
			t.append("_t");
		}
		if (!c->get_static_alloc()) {
			t.append("*");
		}
		t.append(" " + (*it)->get_name());

		// Must happen before the constructor parameters are generated!
		actorname_instance_map[(*it)->get_name()] = (*it);

		if (c->get_static_alloc()) {
			if (c->get_target_language() == Target_Language::c) {
				t.append(" = {");
			}
			else {
				t.append("{");
			}
			t.append(generate_actor_constructor_parameters((*it)->get_name(), true, param_order_map[(*it)->get_identifier()], default_param_maps[(*it)->get_identifier()]));
			t.append("}");
		}

		result.append(t + ";\n");
	}

	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		std::string t = (*it)->get_class();
		if (c->get_target_language() == Target_Language::c) {
			t.append("_t");
		}
		t.append("* ");
		t.append((*it)->get_name());
		result.append(t + ";\n");
	}

	return result;
}

static std::string generate_main(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::map<std::string, std::string> schedulable_instances,
	std::map<std::string, std::map<std::string, std::string>> default_param_maps,
	std::map<std::string, std::vector<std::string>> param_order_map)
{
	std::string result;
	Config* c = Config::getInstance();

	result.append("int " + c->get_globals_prefix() + "main(int argc, char* argv[]) {\n");
	if (c->get_orcc_compat()) {
		result.append("\tparse_command_line_input(argc, argv);\n");
	}

	if (!c->get_static_alloc() || c->get_target_language() == Target_Language::c) {
		//initialize channels
		for (auto it = channel_impl_map.begin(); it != channel_impl_map.end(); ++it) {
			std::string tmp;
			ABI_CHANNEL_INIT(c, tmp, it->first, it->second, channel_type_map[it->first], channel_size_map[it->first], "\t");
			result.append(tmp);
		}
	}

	//initialize actor instances and call their init function
	for (auto it = schedulable_instances.begin(); it != schedulable_instances.end(); ++it) {
		if (c->get_target_language() == Target_Language::cpp) {
			if (!c->get_static_alloc()) {
				result.append("\t" + it->first + " = new ");
				result.append(c->get_globals_prefix() + it->second + "(");
				result.append(generate_actor_constructor_parameters(it->first, false, param_order_map[it->second], default_param_maps[it->second]));
				result.append(");\n");
				result.append("\t" + it->first + "->initialize();\n");
			}
			else {
				result.append("\t" + it->first + ".initialize();\n");
			}
		}
		else {
			if (!c->get_static_alloc()) {
				std::string tmp;
				std::string type = c->get_globals_prefix() + it->second + "_t";
				ABI_ALLOC(c, tmp, it->first, "sizeof(" + type + ")", type, "\t");
				result.append(tmp);
				result.append("\t*" + it->first + " = ("+type+"){" + generate_actor_constructor_parameters(it->first, false, param_order_map[it->second], default_param_maps[it->second]) + "}; \n");
				result.append("\t" + c->get_globals_prefix() + it->second + "_initialize(" + c->get_globals_prefix() + it->first + ");\n");
			}
			else {
				result.append("\t" + c->get_globals_prefix() + it->second + "_initialize(&" + c->get_globals_prefix() + it->first + ");\n");
			}
		}
	}

	//call schedulers
	if (c->get_omp_tasking()) {
		result.append("#pragma omp parallel default(shared)\n");
		result.append("#pragma omp single\n");
		result.append("\t{\n");
		for (auto it = global_scheduling_routines.begin();
			it != global_scheduling_routines.end(); ++it)
		{
			result.append("#pragma omp task\n");
			result.append("\t" + *it + "();\n");
		}
		result.append("\t}\n");
	}
	else if (c->get_cores() > 1) {
		std::vector<std::string> identifiers;
		for (unsigned i = 0; i < c->get_cores(); ++i) {
			std::string tmp;
			std::string identifier;
			ABI_THREAD_CREATE(c, tmp, global_scheduling_routines[i], "\t", identifier);
			result.append(tmp);
			identifiers.push_back(identifier);
		}
		for (auto i = identifiers.begin(); i != identifiers.end(); ++i) {
			std::string tmp;
			ABI_THREAD_START(c, tmp, *i, "\t");
			result.append(tmp);
		}
		for (auto i = identifiers.begin(); i != identifiers.end(); ++i) {
			std::string tmp;
			ABI_THREAD_JOIN(c, tmp, *i, "\t");
			result.append(tmp);
		}
	}
	else {
		result.append("\t" + global_scheduling_routines[0] + "();\n");
	}
	result.append("\treturn 0;\n");
	result.append("}");
	return result;
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::generate_core(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& includes,
	std::map<std::string, std::string> schedulable_instances,
	std::map<std::string, std::map<std::string, std::string>> default_param_maps,
	std::map<std::string, std::vector<std::string>> param_order_map)
{
#ifdef DEBUG_MAIN_GENERATION
	std::cout << "Main Generation." << std::endl;
#endif

	Config* c = Config::getInstance();

	std::string code{};

	if (c->get_target_ABI() == Target_ABI::rtos) {
		code = generate_rtos_main(dpn, opt_data1, opt_data2, map_data, includes, schedulable_instances, default_param_maps, param_order_map);
	}
	else {

		if (map_data->actor_sharing) {
			ABI_ATOMIC_HEADER(c, code);
		}
		if (!c->get_omp_tasking()) {
			std::string tmp;
			ABI_THREAD_HEADER(c, tmp);
			code.append(tmp);
		}
		if (c->get_list_scheduling()) {
			if (c->get_target_language() == Target_Language::c) {
				std::cout << "List scheduling not implemented for C code generation!" << std::endl;
			}
			code.append("#include <vector>\n");
		}
		{
			std::string tmp;
			ABI_ALLOC_HEADER(c, tmp);
			code.append(tmp);
		}

		code.append("\n#define CHANNEL_SIZE " + std::to_string(c->get_FIFO_size()) + "\n");
		code.append("\n//#define PRINT_FIRINGS\n\n");

		std::string include_code;
		for (auto t : includes) {
			include_code.append("#include \"" + t + "\"\n");
		}

		code.append(include_code);

		code.append("\n\n");
		code.append(generate_channels(dpn, opt_data1, opt_data2, map_data));
		code.append("\n\n");
		code.append(generate_actor_instances(dpn, opt_data1, opt_data2, map_data, default_param_maps, param_order_map));
		code.append("\n\n");
		code.append(Scheduling::generate_global_scheduler(dpn, opt_data1, opt_data2, map_data,
			global_scheduling_routines, schedulable_instances));
		code.append("\n\n");
		code.append(generate_main(dpn, opt_data1, opt_data2, map_data, schedulable_instances, default_param_maps, param_order_map));
	}

	std::filesystem::path path{ c->get_target_dir() };
	std::string filename;
	if (c->get_target_language() == Target_Language::c) {
		filename = "main.c";
	}
	else {
		filename = "main.cpp";
	}
	path /= filename;

	std::ofstream output_file{ path };
	if (output_file.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}
	output_file << code;
	output_file.close();

	return std::make_pair("", filename);
}