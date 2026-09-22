#include "RTOS_Core_Sched.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "Config/config.h"
#include "Code_Generation/Code_Generation.hpp"
#include "Code_Generation/Language/C_Cpp/Converter_RVC_Cpp.hpp"
#include "ABI_rtos.hpp"
#include "String_Helper.h"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"
#include <set>

static std::string globals;

void rtos_register_actor_globals(std::string s)
{
	globals.append(s);
	globals.append("\n");
}


/* Map each channel to the concrete channel implementation that is used for this channel. */
static std::map<std::string, std::string> channel_impl_map;
/* Map each channel to the type of the tokens it carries. */
static std::map<std::string, std::string> channel_type_map;
/* Map each channel to its size (number of tokens it can carry). */
static std::map<std::string, std::string> channel_size_map;
/* Map actor_instance_name_port_name to the name of the generated channel. */
static std::map<std::string, std::string> actorport_channel_map;
static std::map<std::string, IR::Actor_Instance*> actorname_instance_map; //Not for composit actors, they carry their parameters inside!

static std::string generate_actor_constructor_parameters(
	std::string name,
	std::vector<std::string> param_order,
	std::map<std::string, std::string> default_params)
{
	std::string result;
	Config* c = Config::getInstance();

	for (auto param_it = param_order.begin();
		param_it != param_order.end(); ++param_it)
	{
		if (param_it != param_order.begin()) {
			result.append(", ");
		}
		if (actorport_channel_map.contains(name + "_" + *param_it)) {
			result.append("&");
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
				// No Parameter value in the network, no default parameter = bug
				throw Code_Generation::Code_Generation_Exception{ "No Parameter value given for " + name + " parameter: " + *param_it };
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
				+ " and " + (*it)->get_sink()->get_name() + "." + (*it)->get_dst_port() + " don't match." };
#else
			std::cout << "WARNING: Types of " + (*it)->get_source()->get_name() + "." + (*it)->get_src_port()
				+ " and " + (*it)->get_sink()->get_name() + "." + (*it)->get_dst_port() + " don't match.\n";
#endif
		}
		actorport_channel_map[(*it)->get_source()->get_name() + "_" + (*it)->get_src_port()] = name;
		actorport_channel_map[(*it)->get_sink()->get_name() + "_" + (*it)->get_dst_port()] = name;

		channel_type_map[name] = typeSource;
		std::string chan_sz;
		if (((*it)->get_specified_size() == c->get_FIFO_size()) || ((*it)->get_specified_size() == 0)) {
			chan_sz = "CHANNEL_SIZE";
		}
		else {
			chan_sz = std::to_string((*it)->get_specified_size());
		}
		channel_size_map[name] = chan_sz;

		std::pair<std::string, std::string> decl;
		decl = ABI_rtos::channel_decl(name, chan_sz, typeSource, true, "");

		channel_impl_map[name] = decl.second;
		result.append(decl.first);
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

	result.append("int " + c->get_globals_prefix() + "main(void) {\n");

	if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
		result.append("\n\tcycle_barrier_init(&cycle_barrier);\n\n");
	}

	//initialize channels
	for (auto it = channel_impl_map.begin(); it != channel_impl_map.end(); ++it) {
		std::string tmp;
		tmp = ABI_rtos::channel_init(it->first, it->second, channel_type_map[it->first], channel_size_map[it->first], "\t");
		result.append(tmp);
	}

	//initialize actor instances and call their init function
	for (auto it = schedulable_instances.begin(); it != schedulable_instances.end(); ++it) {
		result.append("\t" + c->get_globals_prefix() + it->first + "(" + generate_actor_constructor_parameters(it->first, param_order_map[it->second], default_param_maps[it->second]) + "); \n");
		result.append("\t" + c->get_globals_prefix() + it->first + "_initialize();\n");
	}

	if ((c->get_deadline() != 0) || (c->get_release() != 0)) {
		//Give it one MS to initialize all of this, yes, this is a lot, but can be adjusted as needed.
		result.append("\tstart_time = get_time() + MSEC(1);\n");
	}

	result.append("\n\tthr_attr_t tattr;\n");

	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}
		result.append("\tthr_attr_init(&tattr);\n");
		result.append("\ttattr.prio = " + std::to_string((*it)->get_sched_prio()) + ";\n");
		result.append("\ttattr.affinity = CPUMASK_CPU_TO_MASK(" + std::to_string((*it)->get_mapping()) + ");\n");
		result.append("\tthr_create(&" + (*it)->get_name() + "_thrid, &tattr, \"" + (*it)->get_name() + "\", " + c->get_globals_prefix() + (*it)->get_name() + "_schedule, 0); \n");
		result.append("\n");
	}

	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		result.append("\tthr_attr_init(&tattr);\n");
		result.append("\ttattr.prio = " + std::to_string((*it)->get_sched_prio()) + ";\n");
		result.append("\ttattr.affinity = CPUMASK_CPU_TO_MASK(" + std::to_string((*it)->get_mapping()) + ");\n");
		result.append("\tthr_create(&" + (*it)->get_name() + "_thrid, &tattr, \"" + (*it)->get_name() + "\", " + c->get_globals_prefix() + (*it)->get_name() + "_schedule, 0); \n");
		result.append("\n");
	}

	result.append("\tthread_set_priority(THREAD_MYSELF, 10);\n");
	result.append("\tfor (;;) { thread_yield(); };\n");
	result.append("}");
	return result;
}

static void generate_cycle_barrier(
	unsigned num_sources,
	unsigned num_actors)
{
	std::string cycle_barrier_code =
		"#ifndef CYCLE_BARRIER_H\n"
		"#define CYCLE_BARRIER_H\n\n"
		"#include <os.h>\n\n"
		"#define BARRIER_NUM_ACTORS " + std::to_string(num_actors) + "\n"
		"#define BARRIER_NUM_SOURCES " + std::to_string(num_sources) + "\n\n"
		"typedef struct {\n"
		"\tatomic_t actors_registered;\n"
		"\tatomic_t actor[BARRIER_NUM_ACTORS];\n"
		"\tatomic_t sources_seen;\n"
		"\tatomic_t signaling;\n"
		"\tvolatile uid_t src_uid[BARRIER_NUM_SOURCES];\n"
		"\tatomic_t num_sources;\n"
		"\tvolatile unsigned cycle_count;\n"
		"} cycle_barrier_t;\n\n"
		"static inline void cycle_barrier_signal(\n"
		"\tcycle_barrier_t * b)\n"
		"{\n"
		"\tunsigned i;\n"
		"\tif (b->sources_seen < BARRIER_NUM_SOURCES) {\n"
		"\t\treturn;\n"
		"\t}\n"
		"\tfor (i = 0; i < b->actors_registered; ++i) {\n"
		"\t\tif (b->actor[i] != 0) {\n"
		"\t\t\treturn;\n"
		"\t\t}\n"
		"\t}\n"
		"\tif (!atomic_cas(&b->signaling, 0, 1)) {\n"
		"\t\treturn;\n"
		"\t}\n"
		"\tfor (i = 0; i < b->actors_registered; ++i) {\n"
		"\t\tif (b->actor[i] != 0) {\n"
		"\t\t\tb->signaling = 0;\n"
		"\t\t\treturn;\n"
		"\t\t}\n"
		"\t}\n"
		"\tif (!atomic_cas(&(b->sources_seen), BARRIER_NUM_SOURCES, 0)) {\n"
		"\t\tb->signaling = 0;\n"
		"\t\treturn;\n"
		"\t}\n"
		"\t++(b->cycle_count);\n"
		"\tmemory_barrier();\n"
		"\tfor (i = 0; i < b->num_sources; ++i) {\n"
		"\t\tif (b->src_uid[i] != UID_INVALID) {\n"
		"\t\t\tevent_signal(b->src_uid[i]);\n"
		"\t\t}\n"
		"\t}\n"
		"\tb->signaling = 0;\n"
		"}\n\n"
		"static inline void cycle_barrier_quiet(\n"
		"\tcycle_barrier_t * b,\n"
		"\tunsigned i,\n"
		"\tbool_t notify)\n"
		"{\n"
		"\tatomic_and(&(b->actor[i]), 0x1);\n"
		"\tif (notify) {\n"
		"\t\tcycle_barrier_signal(b);\n"
		"\t}\n"
		"}\n\n"
		"static inline void cycle_barrier_busy(\n"
		"\tcycle_barrier_t * b,\n"
		"\tunsigned i)\n"
		"{\n"
		"\tatomic_or(&(b->actor[i]), 0x1);\n"
		"}\n\n"
		"static inline void cycle_barrier_woken(\n"
		"\tcycle_barrier_t * b,\n"
		"\tunsigned i)\n"
		"{\n"
		"\tatomic_write(&(b->actor[i]), 0x2);\n"
		"}\n\n"
		"static inline unsigned cycle_barrier_register_actor(\n"
		"\tcycle_barrier_t * b)\n"
		"{\n"
		"\treturn atomic_fetch_and_add(&(b->actors_registered));\n"
		"}\n\n"
		"static inline void cycle_barrier_register_source(\n"
		"\tcycle_barrier_t * b,\n"
		"\tuid_t src_uid)\n"
		"{\n"
		"\tunsigned i = atomic_fetch_and_add(&(b->num_sources));\n"
		"\tb->src_uid[i] = src_uid;\n"
		"}\n\n"
		"static inline void cycle_barrier_wait(\n"
		"\tcycle_barrier_t * b)\n"
		"{\n"
		"\tuint32_t counter;\n"
		"\tunsigned cycle_count_prev;\n"
		"\tcycle_count_prev = b->cycle_count;\n"
		"\tatomic_inc(&b->sources_seen);\n"
		"\tcycle_barrier_signal(b);\n"
		"\t/* loop only to safeguard spurious wakeups - actually can be removed */\n"
		"\twhile (b->cycle_count == cycle_count_prev) {\n"
		"\t\tevent_wait(TIMEOUT_INFINITE, event_CONSUME_ALL, &counter);\n"
		"\t}\n"
		"}\n\n"
		"static inline void cycle_barrier_init(\n"
		"\tcycle_barrier_t * b)\n"
		"{\n"
		"\tunsigned i;\n"
		"\tfor (i = 0; i < BARRIER_MAX_ACTORS; ++i) {\n"
		"\t\tb->actor[i] = 0;\n"
		"\t}\n"
		"\tfor (i = 0; i < BARRIER_MAX_SOURCES; ++i) {\n"
		"\t\tb->src_uid[i] = UID_INVALID;\n"
		"\t}\n"
		"\tb->num_sources = 0;\n"
		"\tb->sources_seen = 0;\n"
		"\tb->signaling = 0;\n"
		"\tb->cycle_count = 0;\n"
		"\tb->actors_registered = 0;\n"
		"}\n"
		"#endif";

	Config* c = Config::getInstance();

	std::filesystem::path path{ c->get_target_dir() };
	path /= "cycle_barrier.h";

	std::ofstream output_file{ path };
	if (output_file.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}
	output_file << cycle_barrier_code;
	output_file.close();
}

std::string generate_rtos_main(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& includes,
	std::map<std::string, std::string> schedulable_instances,
	std::map<std::string, std::map<std::string, std::string>> default_param_maps,
	std::map<std::string, std::vector<std::string>> param_order_map)
{
	Config* c = Config::getInstance();

	{
		std::filesystem::path path{ c->get_target_dir() };
		path /= "actors.h";

		std::ofstream output_file{ path };
		if (output_file.fail()) {
			throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
		}

		std::string actors_header;

		actors_header.append("#ifndef ACTORS_H\n");
		actors_header.append("#define ACTORS_H\n\n");
		actors_header.append("#include \"channel.h\"\n\n");
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			actors_header.append("#include \"cycle_barrier.h\"\n\n");
			actors_header.append("extern cycle_barrier_t cycle_barrier;\n");
		}

		if ((c->get_deadline() != 0) || (c->get_release() != 0)) {
			actors_header.append("#include <os.h>\n");
			actors_header.append("extern time_t start_time;\n\n");
		}

		actors_header.append(globals);
		actors_header.append("#endif");
		output_file << actors_header;
		output_file.close();
	}

	std::string code;

	code.append("#include <os.h>\n");
	code.append("#include <threads.h>\n");
	code.append("#include \"actors.h\"\n");

	code.append("\n#define CHANNEL_SIZE " + std::to_string(c->get_FIFO_size()) + "\n");

	if ((c->get_deadline() != 0) || (c->get_release() != 0)) {
		code.append("time_t start_time;\n");
	}


	unsigned num_sources = 0;
	unsigned num_actors = 0;
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}
		if ((*it)->get_source()) {
			num_sources++;
		}
		else {
			num_actors++;
		}
	}
	for (auto it = dpn->get_composit_actors().begin();
		it != dpn->get_composit_actors().end(); ++it)
	{
		if ((*it)->get_source()) {
			num_sources++;
		}
		else {
			num_actors++;
		}
	}
	if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
		generate_cycle_barrier(num_sources, num_actors);
		code.append("cycle_barrier_t cycle_barrier;\n");
	}
	
	if (c->get_globals_prefix().empty()) {
		/* if there is a global prefix it should be defined by higher layer */
		code.append("unsigned char stackzone[2 * PAGESIZE * " + std::to_string(num_actors + num_sources) + "];\n\n");
	}

	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_composit_actor() != nullptr) {
			continue;
		}
		if ((*it)->is_deleted()) {
			continue;
		}

		// Must happen before the constructor parameters are generated!
		actorname_instance_map[(*it)->get_name()] = (*it);
	}

	code.append("\n");
	code.append(generate_channels(dpn, opt_data1, opt_data2, map_data));
	code.append("\n\n");
	code.append(generate_main(dpn, opt_data1, opt_data2, map_data, schedulable_instances, default_param_maps, param_order_map));

	return code;
}

static std::string default_local(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	//maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string>& action_guard,
	std::map<std::string, std::string>& action_schedulingCondition_map,
	std::map<std::string, std::string>& action_freeSpaceCondition_map,
	std::map<std::string, std::string>& state_channel_access,
	std::map<std::string, std::string>& action_post_exec_code_map,
	std::string schedule_function_name,
	unsigned loop_count,
	std::string sched_loop,
	bool is_source,
	bool is_sink,
	std::string notifactions)
{
	Config* c = Config::getInstance();

	unsigned deadline = c->get_deadline();
	unsigned release = c->get_release();

	std::string output{ };
	std::string prefix = "\t";
	std::string local_prefix;
	output.append("static tls_t tls;\n");
	if (c->get_use_mutex()) {
		output.append("static mutex_t sched_mutex;\n");
	}
	else if (c->get_use_atomics()) {
		output.append("static atomic_t sched_atomic;\n");
	}
	if ((is_source && (release != 0)) || (is_sink && (deadline != 0))) {
		output.append("static time_t local_time;\n");
	}
	if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1) && !is_source) {
		output.append("static unsigned cycle_barrier_id;\n");
	}

	output.append("static uid_t myself = UID_INVALID;\n\n");
	output.append("void " + schedule_function_name + "(void) {\n");
	output.append("\ttls_init(&tls);\n");
	output.append("\ttls_register(&tls);\n");
	output.append("\tmyself = get_my_uid();\n");
	output.append("\tevent_mask(UID_ALL);\n");
	output.append("\tprio_t myprio = get_my_prio();\n");
	if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
		if (is_source) {
			output.append("\tcycle_barrier_register_source(&cycle_barrier, myself);\n");
		}
		else {
			output.append("\tcycle_barrier_id = cycle_barrier_register_actor(&cycle_barrier);\n");
		}
	}

	if ((is_source && (release != 0)) || (is_sink && (deadline != 0))) {
		output.append("\tlocal_time = start_time;\n");
	}

	if (!is_source) {
		output.append("\tuint32_t counter;\n");
		//output.append("\tevent_wait(TIMEOUT_INFINITE, CONSUME_ALL_EVENTS, &counter);\n");
	}

	output.append("\n\tfor (;;) {\n");

	if (is_sink && (deadline != 0)) {
		output.append("\t\tthread_register_deadline(local_time + " + std::to_string(deadline) + ", THREAD_DEADLINE_ABSOLUTE);\n");
		output.append("\t\tlocal_time += " + std::to_string(release) + ";\n");
	}
	
	if ((is_source || is_sink) && (loop_count > 0)) {
		output.append("\t\tfor (unsigned j = 0; j < " + std::to_string(loop_count) + ";) {\n");
		prefix.append("\t");
	}
	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "\tunsigned firings = 0;\n");
	output.append("#endif\n");
	prefix.append("\t");
	replace_all_substrings(sched_loop, "\t", prefix);
	output.append(sched_loop);

	if (!fsm.empty()) {
		std::string state_enum_compare;
		std::string state_enum_assign;
		state_enum_compare = "state == ";
		state_enum_assign = "state = ";
		std::set<std::string> states = Scheduling::get_all_states(fsm);
		for (auto it = states.begin(); it != states.end(); ++it) {
			if ((it == states.begin())) {
				output.append(prefix + "\tif (" + state_enum_compare + *it + ") {\n");
			}
			else {
				output.append(prefix + "\telse if (" + state_enum_compare + *it + ") {\n");
			}
			//find actions that could be scheduled in this state
			std::vector<std::string> schedulable_actions = find_schedulable_actions(*it, fsm, actions);
			//sort the list of schedulable actions with the comparsion function defined above if a priority block is defined
			if (!priorities.empty()) {
				std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
			}
			local_prefix = prefix + "\t\t";
			//create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				action_it != schedulable_actions.end(); ++action_it)
			{
				std::string action_condition = action_guard[*action_it];
				std::string tmp = action_schedulingCondition_map[*action_it];

				bool cond_non_true = action_condition != "true" && action_condition != "(true)";
				bool sched_non_true = tmp != "true" && tmp != "(true)";

				if (cond_non_true && sched_non_true) {
					tmp.append(" && ");
					tmp.append(action_condition);
					action_condition = tmp;
				}
				else if (sched_non_true) {
					action_condition = tmp;
				}
				if ((action_it == schedulable_actions.begin())) {
					output.append(local_prefix + "if (" + action_condition + ") {\n");
				}
				else {
					output.append(local_prefix + "else if (" + action_condition + ") {\n");
				}
				if (action_freeSpaceCondition_map[*action_it].empty()) {
					output.append(local_prefix + "\t" + *action_it + "(" +
						get_action_in_parameters(*action_it, actions) + "); \n");
					output.append("#ifdef PRINT_FIRINGS\n");
					output.append(local_prefix + "\t++firings;\n");
					output.append("#endif\n");
					output.append(local_prefix + "\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty()) {
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t");
						output.append(t);
					}
				}
				else {
					output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*action_it] + ") {\n");
					output.append(local_prefix + "\t\t" + *action_it + "(" +
						get_action_in_parameters(*action_it, actions) + "); \n");
					output.append("#ifdef PRINT_FIRINGS\n");
					output.append(local_prefix + "\t\t++firings;\n");
					output.append("#endif\n");
					output.append(local_prefix + "\t\t" + state_enum_assign + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
					if (!action_post_exec_code_map[*action_it].empty()) {
						std::string t = action_post_exec_code_map[*action_it];
						replace_all_substrings(t, "\t", local_prefix + "\t\t");
						output.append(t);
					}
					output.append(local_prefix + "\t}\n");
					output.append(local_prefix + "\telse {\n");
					output.append(local_prefix + "\t\tbreak;\n");
					output.append(local_prefix + "\t}\n");
				}
				output.append(local_prefix + "}\n");
			}

			output.append(local_prefix + "else {\n");
			output.append(local_prefix + "\tbreak;\n");
			output.append(local_prefix + "}\n");

			output.append(prefix + "\t}\n");//close state checking if
		}
	}
	else {
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty()) {
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
		}
		local_prefix = prefix + "\t";
		for (auto it = schedulable_actions.begin(); it != schedulable_actions.end(); ++it) {
			std::string action_condition = action_guard[*it];
			std::string tmp = action_schedulingCondition_map[*it];

			bool cond_non_true = (action_condition != "true") && (action_condition != "(true)");
			bool sched_non_true = (tmp != "true") && (tmp != "(true)");

			if (cond_non_true && sched_non_true) {
				tmp.append(" && ");
				tmp.append(action_condition);
				action_condition = tmp;
			}
			else if (sched_non_true) {
				action_condition = tmp;
			}
			if ((it == schedulable_actions.begin())) {
				output.append(local_prefix + "if (" + action_condition + ") {\n");
			}
			else {
				output.append(local_prefix + "else if (" + action_condition + ") {\n");
			}
			if (action_freeSpaceCondition_map[*it].empty()) {
				output.append(local_prefix + "\t" + *it + "(" + get_action_in_parameters(*it, actions) + "); \n");
				output.append("#ifdef PRINT_FIRINGS\n");
				output.append(local_prefix + "\t++firings;\n");
				output.append("#endif\n");
				if (!action_post_exec_code_map[*it].empty()) {
					std::string t = action_post_exec_code_map[*it];
					replace_all_substrings(t, "\t", local_prefix + "\t");
					output.append(t);
				}
			}
			else {
				output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*it] + ") {\n");
				output.append(local_prefix + "\t\t" + *it + "(" + get_action_in_parameters(*it, actions) + "); \n");
				output.append("#ifdef PRINT_FIRINGS\n");
				output.append(local_prefix + "\t\t++firings;\n");
				output.append("#endif\n");
				if (!action_post_exec_code_map[*it].empty()) {
					std::string t = action_post_exec_code_map[*it];
					replace_all_substrings(t, "\t", local_prefix + "\t\t");
					output.append(t);
				}
				output.append(local_prefix + "\t}\n");
				output.append(local_prefix + "\telse {\n");
				output.append(local_prefix + "\t\tbreak;\n");
				output.append(local_prefix + "\t}\n");
			}
			output.append(local_prefix + "}\n");
		}
		output.append(local_prefix + "else {\n");
		output.append(local_prefix + "\tbreak;\n");
		output.append(local_prefix + "}\n");
	}
	output.append(prefix + "}\n");//close inner sched loop

	if ((is_sink || is_source) && (loop_count > 0)) {
		output.append(prefix + "j += sched_loops; \n");
	}

	/* finish cycles */
	if (!notifactions.empty()) {
		replace_all_substrings(notifactions, "\t", prefix);
		output.append(notifactions);
	}

	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "printf(\"%s fired %d times.\\n\", actor_name, firings);\n");
	output.append("#endif\n");

	if (is_source) {
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			output.append(prefix + "cycle_barrier_wait(&cycle_barrier);\n");
		}
		else {
			output.append(prefix + "thread_yield();\n");
		}
	}
	else {
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			output.append(prefix + "cycle_barrier_quiet(&cycle_barrier, cycle_barrier_id, " + (is_sink ? "1" : "0") + "); \n");
		}
		output.append(prefix + "event_wait(TIMEOUT_INFINITE, CONSUME_ALL_EVENTS, &counter);\n");
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			output.append(prefix + "cycle_barrier_woken(&cycle_barrier, cycle_barrier_id);\n");
		}
	}

	if ((is_sink || is_source) && (loop_count > 0)) {
		output.append("\t\t}\n");// close middle scheduling loop
	}
	if (is_sink) {
		if (deadline != 0) {
			output.append("\t\tthread_stop(THREAD_MYSELF);\n");
		}
	}
	else if (is_source) {
		if (release != 0) {
			output.append("\t\tlocal_time += " + std::to_string(release) + ";\n");
			output.append("\t\tthread_register_deadline(local_time, THREAD_DEADLINE_ABSOLUTE);\n");
		}
	}

	output.append("\t}\n");// close outer scheduling loop
	output.append("}\n\n");// close scheduler method

	/* Avoid true as it might not be defined. */
	replace_all_substrings(output, "(true)", "(1)");

	return output;
}

static std::string default_schedcheck(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	//maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string>& action_guard,
	std::map<std::string, std::string>& action_schedulingCondition_map,
	std::map<std::string, std::string>& action_freeSpaceCondition_map,
	std::map<std::string, std::string>& state_channel_access)
{
	std::string output{ };
	std::string prefix;
	std::string local_prefix;

	if (false && !fsm.empty()) {
		std::string state_enum_compare;
		std::string state_enum_assign;
		state_enum_compare = "state == ";
		state_enum_assign = "state = ";
		std::set<std::string> states = Scheduling::get_all_states(fsm);
		for (auto it = states.begin(); it != states.end(); ++it) {
			if ((it == states.begin())) {
				output.append(prefix + "\tif (" + state_enum_compare + *it + ") {\n");
			}
			else {
				output.append(prefix + "\telse if (" + state_enum_compare + *it + ") {\n");
			}
			//find actions that could be scheduled in this state
			std::vector<std::string> schedulable_actions = find_schedulable_actions(*it, fsm, actions);
			//sort the list of schedulable actions with the comparsion function defined above if a priority block is defined
			if (!priorities.empty()) {
				std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
			}
			local_prefix = prefix + "\t\t";
			//create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				action_it != schedulable_actions.end(); ++action_it)
			{
				std::string action_condition = action_guard[*action_it];
				std::string tmp = action_schedulingCondition_map[*action_it];

				bool cond_non_true = action_condition != "true" && action_condition != "(true)";
				bool sched_non_true = tmp != "true" && tmp != "(true)";

				if (cond_non_true && sched_non_true) {
					tmp.append(" && ");
					tmp.append(action_condition);
					action_condition = tmp;
				}
				else if (sched_non_true) {
					action_condition = tmp;
				}
				if ((action_it == schedulable_actions.begin())) {
					output.append(local_prefix + "if (" + action_condition + ") {\n");
				}
				else {
					output.append(local_prefix + "else if (" + action_condition + ") {\n");
				}
				output.append(local_prefix + "\t" + "trigger = 1;\n");
				output.append(local_prefix + "}\n");
			}

			output.append(prefix + "\t}\n");//close state checking if
		}
	}
	else {
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty()) {
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
		}
		local_prefix = prefix + "\t";
		for (auto it = schedulable_actions.begin(); it != schedulable_actions.end(); ++it) {
			std::string action_condition = action_guard[*it];
			std::string tmp = action_schedulingCondition_map[*it];

			bool cond_non_true = (action_condition != "true") && (action_condition != "(true)");
			bool sched_non_true = (tmp != "true") && (tmp != "(true)");

			if (cond_non_true && sched_non_true) {
				tmp.append(" && ");
				tmp.append(action_condition);
				action_condition = tmp;
			}
			else if (sched_non_true) {
				action_condition = tmp;
			}
			if ((it == schedulable_actions.begin())) {
				output.append(local_prefix + "if (" + action_condition + ") {\n");
			}
			else {
				output.append(local_prefix + "else if (" + action_condition + ") {\n");
			}
			output.append(local_prefix + "\t" + "trigger = 1;\n");
			output.append(local_prefix + "}\n");
		}
	}
	
	/* Avoid true as it might not be defined. */
	replace_all_substrings(output, "(true)", "(1)");

	return output;
}


std::string generate_rtos_scheduler(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	//maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string>& action_guard,
	std::map<std::string, std::string>& action_schedulingCondition_map,
	std::map<std::string, std::string>& action_freeSpaceCondition_map,
	std::map<std::string, std::string>& state_channel_access,
	std::map<std::string, std::string>& action_post_exec_code_map,
	std::string schedule_function_name,
	unsigned loopcount,
	std::string sched_loop,
	bool is_source,
	bool is_sink)
{
	Config* c = Config::getInstance();
	std::string ret;

	rtos_register_actor_globals("void " + schedule_function_name + "(void);\n");

	std::map<unsigned, unsigned> inports;
	unsigned feedback_count = 0;
	std::map<std::string, bool> outports;
	std::map<unsigned, std::string> names;
	for (auto a : actions) {
		for (auto t : a.second) {
			if (t.in) {
				unsigned arg = t.arg;
				unsigned rate = t.elements;
				if (inports.contains(arg)) {
					if (inports[arg] > rate) {
						inports[arg] = rate;
					}
				}
				else {
					inports[arg] = rate;
					names[arg] = t.channel_name;
					if (arg >= 1000) {
						feedback_count++;
					}
				}
			}
			else {
				outports[t.channel_name] = (t.arg >= 1000);
			}
		}
	}

	std::string notifications;
	if (!outports.empty()) {
		notifications.append("\tset_my_prio(100);\n");
	}
	for (auto x : names) {
		std::string tmp = ABI_rtos::channel_read_notification(x.second);
		notifications.append("\t" + tmp + ";\n");
	}
	std::string others_branch;
	for (auto x : outports) {
		std::string tmp = ABI_rtos::channel_write_notification(x.first);
		others_branch.append("\t" + tmp + ";\n");
	}
	notifications.append(others_branch);
	if (!outports.empty()) {
		notifications.append("\tset_my_prio(myprio);\n");
	}

	ret = default_local(actions, fsm, priorities, action_guard, action_schedulingCondition_map, action_freeSpaceCondition_map, state_channel_access,
						action_post_exec_code_map, schedule_function_name, loopcount, sched_loop, is_source, is_sink, notifications);

	std::string schedcheck = "static void schedcheck(unsigned arg, unsigned count)\n{\n";

	if (c->get_rtos_strategy() == RTOS_Strategy::basic) {
		schedcheck.append("\tif (count != 0) {\n");
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			schedcheck.append("\t\tcycle_barrier_busy(&cycle_barrier, cycle_barrier_id);\n");
		}
		schedcheck.append("\t\tevent_signal(myself);\n");
		schedcheck.append("\t}\n");
	}
	else if (c->get_rtos_strategy() == RTOS_Strategy::counting) {
		bool first = true;
		bool feedback = false;
		std::string cond;
		std::string reset;
		if (!c->get_use_atomics()) {
			if (c->get_use_mutex()) {
				schedcheck.append("\tmutex_lock(&sched_mutex, TIMEOUT_INFINITE);\n");
			}
			for (auto x : names) {
				if (x.first >= 1000) {
					if (!feedback) {
						schedcheck.append("\tif ((arg >= 1000) && (count > 0)) {\n");
						if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
							schedcheck.append("\t\tcycle_barrier_busy(&cycle_barrier, cycle_barrier_id);\n");
						}
						schedcheck.append("\t\tevent_signal(myself);\n");
						schedcheck.append("\t}\n");
						feedback = true;
					}
				}
				ret.append("static volatile bool_t ");
				ret.append(x.second);
				ret.append("_flag = 0;\n");
				reset.append("\t\t" + x.second + "_flag = 0;\n");

				if (first) {
					cond.append(x.second + "_flag");
					schedcheck.append("\tif (arg == " + std::to_string(x.first) + ") {\n");
					schedcheck.append("\t\t" + x.second + "_flag = 1;\n");
					schedcheck.append("\t}\n");
					first = false;
				}
				else {
					cond.append("&&" + x.second + "_flag");
					schedcheck.append("\telse if (arg == " + std::to_string(x.first) + ") {\n");
					schedcheck.append("\t\t" + x.second + "_flag = 1;\n");
					schedcheck.append("\t}\n");
				}
			}
			schedcheck.append("\tmemory_barrier();\n");
			schedcheck.append("\tif (" + cond + ") {\n");
			schedcheck.append(reset);
		}
		else {
			if (names.size() - feedback_count >= 32) {
				throw Code_Generation::Code_Generation_Exception{ "Too many input ports for atomic scheduling." };
			}
			/* Feedback channels use arg >= 1000 and shall not contribute. */
			schedcheck.append("\tif (arg < 32) {\n");
			schedcheck.append("\t\tatomic_or(&sched_atomic, 1U << arg);\n");
			schedcheck.append("\t}\n");
			schedcheck.append("\tif (atomic_cas(&sched_atomic, (1U <<" + std::to_string(names.size() - feedback_count) + ") - 1, 0)) { \n");
		}
		if (c->get_use_mutex()) {
			schedcheck.append("\t\tmutex_unlock(&sched_mutex);\n");
		}
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			schedcheck.append("\t\tcycle_barrier_busy(&cycle_barrier, cycle_barrier_id);\n");
		}
		schedcheck.append("\t\tevent_signal(myself);\n");
		schedcheck.append("\t}\n");
		if (c->get_use_mutex()) {
			schedcheck.append("\telse {\n");
			schedcheck.append("\t\tmutex_unlock(&sched_mutex);\n");
			schedcheck.append("\t}\n");
		}
	}
	else if (c->get_rtos_strategy() == RTOS_Strategy::schedcheck) {
		schedcheck.append("\tbool_t trigger = 0;\n");
		if (c->get_use_mutex()) {
			schedcheck.append("\tmutex_lock(&sched_mutex, TIMEOUT_INFINITE);\n");
		}
		for (auto x : inports) {
			schedcheck.append("\tunsigned " + names[x.first] + "_size;\n");
			schedcheck.append("\t" + names[x.first] + "_size = " + ABI_rtos::channel_size(names[x.first]) + ";\n");
		}

		schedcheck.append(default_schedcheck(actions, fsm, priorities, action_guard, action_schedulingCondition_map, action_freeSpaceCondition_map, state_channel_access));
		if (c->get_use_mutex()) {
			schedcheck.append("\tmutex_unlock(&sched_mutex);\n");
		}
		schedcheck.append("\tif (trigger) {\n");
		if ((c->get_rtos_sched_cycles() > 1) && (c->get_cores() > 1)) {
			schedcheck.append("\t\tcycle_barrier_busy(&cycle_barrier, cycle_barrier_id);\n");
		}
		schedcheck.append("\t\tevent_signal(myself);\n");
		schedcheck.append("\t}\n");
	}

	schedcheck.append("}\n");

	if (!inports.empty()) {
		ret.append(schedcheck);
	}

	return ret;
}

void rtos_generate_makefile(
	IR::Dataflow_Network* dpn)
{
	Config* c = Config::getInstance();

	std::filesystem::path path{ c->get_target_dir() };
	path /= "makefile.defs";

	std::ofstream output_file{ path };
	if (output_file.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}

	std::string mk;
	if (c->get_rtos_subdir().empty()) {
		mk = "APPLIST = " + dpn->get_name() + ".elf\n\n";
		mk += "MODLIST =\\\n";

		for (auto x : dpn->get_actor_instances()) {
			if ((x->get_composit_actor() != nullptr) || (x->is_deleted())) {
				continue;
			}
			mk.append("\t" + x->get_identifier() + " \\\n");
		}
		for (auto x : dpn->get_composit_actors()) {
			mk.append("\t" + x->get_class() + " \\\n");
		}
		mk.append("\tmain\n");

		mk += dpn->get_name() + ".elf: $(addprefix $(ODIR)/, $(addsuffix .o, $(MODLIST)))";
	}
	else {
		mk = "MODLIST +=\\\n";
		for (auto x : dpn->get_actor_instances()) {
			if ((x->get_composit_actor() != nullptr) || (x->is_deleted())) {
				continue;
			}
			mk.append("\t" + c->get_rtos_subdir() + "/" + x->get_identifier() + " \\\n");
		}
		for (auto x : dpn->get_composit_actors()) {
			mk.append("\t" + c->get_rtos_subdir() + "/" + x->get_class() + " \\\n");
		}
		mk.append("\t" + c->get_rtos_subdir() + "/main");
	}

	output_file << mk;
	output_file.close();
}