#include "Code_Generation.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include "Scheduling/Scheduling.hpp"
#include <string>
#include <fstream>
#include <map>

/* Map each channel to the concrete channel implementation that is used for this channel. */
static std::map<std::string, std::string> channel_impl_map;
/* Map each channel to the type of the tokens it carries. */
static std::map<std::string, std::string> channel_type_map;
/* Map each channel to its size (number of tokens it can carry). */
static std::map<std::string, std::string> channel_size_map;
static std::map<std::string, Actor_Conversion_Data*> actor_data_map;
/* Map actor_instance_name_port_name to the name of the generated channel. */
static std::map<std::string, std::string> actorport_channel_map;
static std::map<std::string, IR::Actor_Instance*> actorname_instance_map; //Not for composit actors, they carry their parameters inside!
static std::vector<std::string> global_scheduling_routines;


static std::string find_channel_type(
	std::string port_name,
	std::vector<IR::Buffer_Access>& ports)
{
	for (auto it = ports.begin(); it != ports.end(); ++it) {
		if (it->buffer_name == port_name) {
			return it->type;
		}
	}
	// this cannot happen as this is detected early during network reading
	throw Code_Generation::Code_Generation_Exception{ "Cannot find type for port." };
}

static std::string generate_actor_constructor_parameters(
	std::string name,
	Actor_Conversion_Data *data,
	bool static_alloc)
{
	std::string result;

	result.append("\"" + name + "\"");
	for (auto param_it = data->get_parameter_order().begin();
		param_it != data->get_parameter_order().end(); ++param_it)
	{
		result.append(", ");
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
			else if (instance->get_conversion_data().get_default_parameter_map().contains(*param_it)) {
				result.append(instance->get_conversion_data().get_default_parameter_map()[*param_it]);
			}
			else {
				if (instance->is_port(*param_it)) {
					result.append("nullptr");
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

static std::string generate_channels(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	std::string result;
	Config* c = c->getInstance();
	for (auto it = dpn->get_edges().begin();
		it != dpn->get_edges().end(); ++it)
	{
		IR::Actor_Instance* source = it->get_source();
		IR::Actor_Instance* sink = it->get_sink();
		if ((source->get_composit_actor() != nullptr)
			&& (source->get_composit_actor() == sink->get_composit_actor()))
		{
			// This is just an edge inside a cluster, no need to create a channel object for it.
			continue;
		}
		if (it->is_deleted()) {
			continue;
		}

		std::string name;

		name = it->get_src_id() + "_" + it->get_src_port() + "_" + it->get_dst_id() + "_" + it->get_dst_port();

		if (channel_impl_map.contains(name)) {
			//just a sanity check, this cannot happen I think
			std::cout << "ERROR: Determined channel name that is already in use: " << name << std::endl;
			exit(5);
		}

		channel_impl_map[name] = "Data_Channel";
		std::string typeSource = find_channel_type(it->get_src_port(), it->get_source()->get_actor()->get_out_buffers());
		std::string typeSink = find_channel_type(it->get_dst_port(), it->get_sink()->get_actor()->get_in_buffers());
		if (typeSource != typeSink) {
			throw Code_Generation::Code_Generation_Exception{
				"Types of " + it->get_source()->get_name() + "." + it->get_src_port()
				+ " and " + it->get_sink()->get_name() + "." + it->get_dst_port() + " don't match."};
		}
		if (c->get_static_alloc()) {
			result.append("Data_Channel<" + typeSource + "> " + name);
		}
		else {
			result.append("Data_Channel<" + typeSource + "> *" + name + "; \n");
		}
		channel_type_map[name] = typeSource;
		if (it->get_specified_size() == c->get_FIFO_size()) {
			channel_size_map[name] = "CHANNEL_SIZE";
		}
		else {
			channel_size_map[name] = std::to_string(it->get_specified_size());
		}

		if (c->get_static_alloc()) {
			result.append("{" + channel_size_map[name] + "};\n");
		}

		actorport_channel_map[it->get_source()->get_name() + "_" + it->get_src_port()] = name;
		actorport_channel_map[it->get_sink()->get_name() + "_" + it->get_dst_port()] = name;
	}

	return result;
}

static std::string generate_actor_instances(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	std::string result;
	Config* c = c->getInstance();

	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}

		std::string t = (*it)->get_conversion_data().get_class_name();
		if (!c->get_static_alloc()) {
			t.append("*");
		}
		t.append(" " + (*it)->get_name());

		// Must happen before the constructor parameters are generated!
		actor_data_map[(*it)->get_name()] = (*it)->get_conversion_data_ptr();
		actorname_instance_map[(*it)->get_name()] = (*it);

		if (c->get_static_alloc()) {
			t.append("{");
			t.append(generate_actor_constructor_parameters((*it)->get_name(), (*it)->get_conversion_data_ptr(), true));
			t.append("}");
		}

		result.append(t + ";\n");
	}

	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		std::string t = (*it)->get_conversion_data().get_class_name();
		t.append("* ");
		t.append((*it)->get_name());
		result.append(t + ";\n");
		actor_data_map[(*it)->get_name()] = (*it)->get_conversion_data_ptr();
	}

	return result;
}

static std::string generate_main(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	std::string result;
	Config* c = c->getInstance();

	result.append("int main(int argc, char* argv[]) {\n");
	if (c->get_orcc_compat()) {
		result.append("\tparse_command_line_input(argc, argv);\n");
	}


	if (!c->get_static_alloc()) {
		//initialize channels
		for (auto it = channel_impl_map.begin(); it != channel_impl_map.end(); ++it) {

			std::string t = it->first + " = new " + it->second + "<" + channel_type_map[it->first]
				+ ">(" + channel_size_map[it->first] + ");";
			result.append("\t" + t + "\n");
		}


		//initialize actor instances and call their init function
		for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it) {
			result.append("\t" + it->first + " = new ");
			result.append(it->second->get_class_name() + "(");
			result.append(generate_actor_constructor_parameters(it->first, it->second, false));
			result.append(");\n");
			result.append("\t" + it->first + "->init();\n");
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
	else {
		unsigned i;
		for (i = 0; i < (c->get_cores()-1); ++i) {
			result.append("\tstd::thread t" + std::to_string(i) + "(" + global_scheduling_routines[i] + ");\n");
		}
		result.append("\t" + global_scheduling_routines[i] + "();\n");
	}
	result.append("\treturn 0;\n");
	result.append("}");
	return result;
}

void Code_Generation::generate_core(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::string include_code)
{
#ifdef DEBUG_MAIN_GENERATION
	std::cout << "Main Generation." << std::endl;
#endif

	Config* c = c->getInstance();

	std::string code{};

	if (map_data->actor_sharing) {
		code.append("#include <atomic>\n");
	}
	if (!c->get_omp_tasking()) {
		code.append("#include <thread>\n");
	}
	if (c->get_list_scheduling()) {
		code.append("#include <vector>\n");
	}

	code.append("\n#define CHANNEL_SIZE " + std::to_string(c->get_FIFO_size()) + "\n");
	code.append("\n//#define PRINT_FIRINGS\n\n");

	code.append(include_code);

	code.append("\n\n");
	code.append(generate_channels(dpn, opt_data1, opt_data2, map_data));
	code.append("\n\n");
	code.append(generate_actor_instances(dpn, opt_data1, opt_data2, map_data));
	code.append("\n\n");
	code.append(Scheduling::generate_global_scheduler(dpn, opt_data1, opt_data2, map_data,
		global_scheduling_routines, actor_data_map));
	code.append("\n\n");
	code.append(generate_main(dpn, opt_data1, opt_data2, map_data));

	std::string path{ c->get_target_dir() };

	std::ofstream output_file{ path + "\\main.cpp" };
	if (output_file.bad()) {
		throw Code_Generation_Exception{ "Cannot open the file " + path + "\\main.cpp" };
	}
	output_file << code;
	output_file.close();
}