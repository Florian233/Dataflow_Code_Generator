#include "Scheduling.hpp"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"
#include "Config/debug.h"
#include "Config/config.h"
#include <algorithm>
#include <tuple>
#include "String_Helper.h"
#include "ABI/abi.hpp"
#include "Converter_RVC_Cpp.hpp"
#include <iostream>
#include "ABI/RTOS/RTOS_Core_Sched.hpp"

static std::string default_local(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix,
	//maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string>& action_guard,
	std::map<std::string, std::string>& action_schedulingCondition_map,
	std::map<std::string, std::string>& action_freeSpaceCondition_map,
	std::map<std::string, std::string>& state_channel_access,
	std::map<std::string, std::string>& action_post_exec_code_map,
	std::string loop_exp,
	bool round_robin,
	std::string schedule_function_name,
	std::string schedule_function_parameter,
	bool no_else)
{
	Config* c = Config::getInstance();
	std::string output{ };
	std::string local_prefix;
	std::string buffered_prefix = prefix;
	bool static_rate = (input_classification != Actor_Classification::dynamic_rate);
	output.append(prefix + "void " + schedule_function_name + "(" + schedule_function_parameter + ") { \n");
	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "\tunsigned firings = 0;\n");
	output.append("#endif\n");
	if (!round_robin) {
		output.append(loop_exp);
		prefix.append("\t");
	}
	if (!fsm.empty()) {
		std::string state_enum_compare;
		std::string state_enum_assign;
		if (c->get_target_language() == Target_Language::c) {
			state_enum_compare = "_g->state == ";
			state_enum_assign = "_g->state = ";
		}
		else if (c->get_target_language() == Target_Language::cpp) {
			state_enum_compare = "state == FSM::";
			state_enum_assign = "state = FSM::";
		}
		std::set<std::string> states = Scheduling::get_all_states(fsm);
		for (auto it = states.begin(); it != states.end(); ++it) {
			if ((it == states.begin()) || no_else) {
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
			if (static_rate) {
				if (!schedulable_actions.empty()) {
					std::string channel_prefetch = state_channel_access[*it];
					replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t\t");
					output.append(prefix + "\t\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {\n");
					output.append(channel_prefetch);
				}
				local_prefix = prefix + "\t\t\t";
			}
			else {
				local_prefix = prefix + "\t\t";
			}
			//create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				action_it != schedulable_actions.end(); ++action_it)
			{
				std::string action_condition = action_guard[*action_it];
				if (!static_rate) {
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
				}
				if ((action_it == schedulable_actions.begin()) || no_else) {
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
					if (round_robin) {
						output.append(local_prefix + "\t\treturn;\n");
					}
					else {
						output.append(local_prefix + "\t\tbreak;\n");
					}
					output.append(local_prefix + "\t}\n");
				}
				output.append(local_prefix + "}\n");
			}
			if (!no_else) {
				output.append(local_prefix + "else {\n");
				if (round_robin) {
					output.append(local_prefix + "\treturn;\n");
				}
				else {
					output.append(local_prefix + "\tbreak;\n");
				}
				output.append(local_prefix + "}\n");
			}
			if (static_rate && !schedulable_actions.empty()) {
				output.append(prefix + "\t\t}\n");
				output.append(prefix + "\t\telse {\n");
				if (round_robin) {
					output.append(prefix + "\t\t\treturn;\n");
				}
				else {
					output.append(prefix + "\t\t\tbreak;\n");
				}
				output.append(prefix + "\t\t}\n");
			}

			output.append(prefix + "\t}\n");//close state checking if
		}
	}
	else {
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty()) {
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
		}
		if (static_rate) {
			// We only need to check size once
			if (!schedulable_actions.empty()) {
				output.append(prefix + "\tif (" + action_schedulingCondition_map[schedulable_actions.front()] + ") {\n");
				std::string channel_prefetch = state_channel_access[""];
				replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t");
				output.append(channel_prefetch);
			}
			local_prefix = prefix + "\t\t";
		}
		else {
			local_prefix = prefix + "\t";
		}
		for (auto it = schedulable_actions.begin(); it != schedulable_actions.end(); ++it) {
			std::string action_condition = action_guard[*it];
			if (!static_rate) {
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
			}
			if ((it == schedulable_actions.begin()) || no_else) {
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
				if (round_robin) {
					output.append(local_prefix + "\t\treturn;\n");
				}
				else {
					output.append(local_prefix + "\t\tbreak;\n");
				}
				output.append(local_prefix + "\t}\n");
			}
			output.append(local_prefix + "}\n");
		}
		if (!no_else) {
			output.append(local_prefix + "else {\n");
			if (round_robin) {
				output.append(local_prefix + "\treturn;\n");
			}
			else {
				output.append(local_prefix + "\tbreak;\n");
			}
			output.append(local_prefix + "}\n");
		}
		if (static_rate && !schedulable_actions.empty()) {
			output.append(prefix + "\t}\n");
			output.append(prefix + "\telse {\n");
			if (round_robin) {
				output.append(prefix + "\t\treturn;\n");
			}
			else {
				output.append(prefix + "\t\tbreak;\n");
			}
			output.append(prefix + "\t}\n");
		}
	}
	if (!round_robin) {
		prefix = buffered_prefix;
		output.append(prefix + "\t}\n");//close for(;;) loop
	}
	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "\t");
	if (c->get_target_language() == Target_Language::c) {
		output.append("printf(\"%s fired %d times.\\n\", actor_name, firings);\n");
	}
	else if (c->get_target_language() == Target_Language::cpp) {
		output.append("std::cout << actor_name << \" fired \" << firings << \" times\" << std::endl;\n");
	}
	output.append("#endif\n");
	output.append(prefix + "}\n");// close scheduler method

	if (c->get_target_language() == Target_Language::c) {
		/* Avoid true as it might not be defined. */
		replace_all_substrings(output, "(true)", "(1)");
	}

	return output;
}


/* Replace channel variables used in the guards by either fetched local variables
 * or prefetch function calls.
 */
static std::string guard_var_replacement(
	AST::Action *action,
	std::map<std::string, std::string>& replacement_map)
{
	std::string guard;
	bool first = true;
	for (auto e : action->guards) {
		if (!first) {
			guard.append(" && ");
		}
		first = false;
		guard.append("(" + Converter_RVC_Cpp::convert_expression(e, replacement_map, std::map<std::string, std::string>()) + ")");
	}
	return guard;
}

/* Update the guard conditions of the actions and generate code to fetch tokens from channels.
 * In the static cases (also cyclo-static etc.) code to fetch the tokens and store it in local
 * variables is generated, the guard conditions are manipulated to use these local variables.
 * Otherwise the guard conditions are manipulated to use the prefetch functionality of the channel
 * to compute the guard condition.
 * The guards are adjusted in action_guard and the code to fetch the tokens is returned as
 * map, mapping the state to the fetch code. If no FSM is defined the code has the empty string as key.
 */
static std::map<std::string, std::string> get_scheduler_channel_access(
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data> >& actions,
	std::map<std::string, AST::Action*>& action_guard,
	Actor_Classification input_classification,
	std::map<std::string, std::vector<std::string>>& actions_per_state,
	std::map<std::string, std::string> replacements,
	std::map<std::string, std::string>& converted_guards)
{
	// Map the channel read to local variable for each state if required (not in the dynamic case)
	std::map<std::string, std::string> output;
	Config* c = Config::getInstance();

	// It is dynamic, hence, we must prefetch
	for (auto action_it = actions.begin();
		action_it != actions.end(); ++action_it)
	{
		std::map<std::string, std::string> replacement_map;
		if (c->get_target_language() == Target_Language::c) {
			replacement_map = replacements;
		}

		for (auto sched_data_it = action_it->second.begin();
			sched_data_it != action_it->second.end(); ++sched_data_it)
		{
			// One scheduling data entry per accessed channel
			unsigned index = 0;
			if (action_guard.contains(action_it->first) && !(action_guard[action_it->first]->guards.empty())) {
				for (auto var_it = sched_data_it->var_names.begin();
					var_it != sched_data_it->var_names.end(); ++var_it)
				{
					if (sched_data_it->unused_channel) {
						if (sched_data_it->repeat) {
							for (unsigned i = 0; i < sched_data_it->elements; ++i) {
								std::string r = *var_it + "[" + std::to_string(i) + "]";
								replacement_map[r] = "0";
							}
						}
						else {
							std::string r = *var_it;
							// Just use a dummy value if we cannot access this channel!
							replacement_map[r] = "0";
						}
					}
					else if (sched_data_it->repeat) {
						size_t repeat_val = sched_data_it->elements / sched_data_it->var_names.size();
						for (size_t i = 0; i < repeat_val; ++i) {
							std::string r = *var_it + "[" + std::to_string(i) + "]";
							std::string tmp;
							std::string offset = std::to_string(i * sched_data_it->var_names.size() + index);
							ABI_CHANNEL_PREFETCH(c, tmp, sched_data_it->channel_name, offset)
							std::string n = tmp;
							replacement_map[r] = n;
						}
					}
					else {
						std::string r = *var_it;
						std::string tmp;
						std::string offset = std::to_string(index);
						ABI_CHANNEL_PREFETCH(c, tmp, sched_data_it->channel_name, offset)
						std::string n = tmp;
						replacement_map[r] = n;
					}
					++index;
				}
			}
		}

		std::string replaced_guard = guard_var_replacement(action_guard[action_it->first], replacement_map);
		converted_guards[action_it->first] = replaced_guard;
	}
	

	return output;
}


std::string Scheduling::generate_local_scheduler(
	std::map<std::string, AST::Action*>& action_guard,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix,
	std::string schedule_function_name,
	std::string schedule_function_parameter,
	std::map<std::string, std::vector< Channel_Schedule_Data>>& actions,
	std::map<std::string, std::string> replacements,
	unsigned scheduling_loop_bound,
	bool no_else,
	bool is_source,
	bool is_sink)
{
	Config* c = Config::getInstance();

#ifdef DEBUG_SCHEDULER_GENERATION
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		std::cout << "Action " << it->first << " scheduling data:" << std::endl;

		for (auto d = it->second.begin(); d != it->second.end(); ++d) {
			std::cout << "Channel name: " << d->channel_name << " Type: " << d->type
				<< " Num: " << d->elements << " Variables:";
			for (auto var_it = d->var_names.begin(); var_it != d->var_names.end(); ++var_it) {
				std::cout <<" " << *var_it;
			}
			if (d->in) {
				std::cout << "; input." << std::endl;
			}
			else {
				std::cout << "; output." << std::endl;
			}
		}
	}
#endif

	std::map<std::string, std::string> action_schedulingCondition_map;
	std::map<std::string, std::string> action_freeSpaceCondition_map;
	std::map<std::string, std::string> action_post_exec_map;

	std::map<std::string, std::vector<std::string>> actions_per_state;
	std::string sched_loop;

	for (auto it : get_all_states(fsm)) {
		std::vector<std::string> sched_actions = find_schedulable_actions(it, fsm, actions);
		actions_per_state[it] = sched_actions;
	}

#ifdef DEBUG_SCHEDULER_GENERATION
	for (auto s : actions_per_state) {
		std::cout << "Actions for state: " << s.first << ": ";
		for (auto a : s.second) {
			std::cout << a << " ";
		}
		std::cout << std::endl;
	}
#endif

	std::map<std::string, std::string> converted_guard_conditions;
	std::map<std::string, std::string> state_channel_access = 
		get_scheduler_channel_access(actions, action_guard, input_classification, actions_per_state, replacements, converted_guard_conditions);

	// Set guard to (true) if no guard is specified as this avoids checking during code generation
	// whether a guard is used or not. The C-Compiler can optimize this.
	for (auto it = converted_guard_conditions.begin(); it != converted_guard_conditions.end(); ++it) {
		if (it->second.empty()) {
			it->second = "(true)";
		}
		else {
			it->second = "(" + it->second + ")";
		}
	}

	if (c->get_optimize_scheduling() || (c->get_target_ABI() == Target_ABI::rtos)) {
		std::set<std::string> in_channels;
		std::set<std::string> out_channels;
		for (auto action_it = actions.begin(); action_it != actions.end(); ++action_it) {
			for (auto sched_data_it = action_it->second.begin();
				sched_data_it != action_it->second.end();
				++sched_data_it)
			{
				if (sched_data_it->unused_channel) {
					continue;
				}
				if (sched_data_it->in) {
					if (in_channels.find(sched_data_it->channel_name) == in_channels.end()) {
						in_channels.insert(sched_data_it->channel_name);
					}
				}
				else {
					if (out_channels.find(sched_data_it->channel_name) == out_channels.end()) {
						out_channels.insert(sched_data_it->channel_name);
					}
				}
			}
		}
		for (auto it = in_channels.begin(); it != in_channels.end(); ++it) {
			std::string tmp;
			ABI_CHANNEL_SIZE(c, tmp, *it)
			sched_loop.append(prefix + "\tunsigned " + *it + "_size = " + tmp + ";\n");
		}
		for (auto it = out_channels.begin(); it != out_channels.end(); ++it) {
			std::string tmp;
			ABI_CHANNEL_FREE(c, tmp, *it)
			sched_loop.append(prefix + "\tunsigned " + *it + "_free = " + tmp+ ";\n");
		}
		sched_loop.append("\n");
	}

	if (scheduling_loop_bound != 0) {
		sched_loop.append(prefix + "\tunsigned sched_loops;\n");
		sched_loop.append(prefix + "\tfor (sched_loops = 0; sched_loops < "
			+ std::to_string(scheduling_loop_bound) + "; ++sched_loops) {\n");
	}
	else {
		sched_loop.append(prefix + "\tfor (;;) {\n");
	}

	//init with empty first so we don't need to care later and keep the guarantee that every action is in there
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		action_schedulingCondition_map[it->first] = "";
		action_freeSpaceCondition_map[it->first] = "";
		action_post_exec_map[it->first] = "";
	}

	/* Determine input channel size checks for each action and output channel free space checks.
	 * They are stored in different maps as insufficient output channel space shall not influence
	 * the scheduling and is therefore treated differently.
	 */
	for (auto action_it = actions.begin(); action_it != actions.end(); ++action_it) {
		for (auto sched_data_it = action_it->second.begin();
			sched_data_it != action_it->second.end();
			++sched_data_it)
		{
			if (sched_data_it->unused_channel) {
				continue;
			}
			if (sched_data_it->in) {
				if (!action_schedulingCondition_map[action_it->first].empty()) {
					action_schedulingCondition_map[action_it->first].append(" && ");
				}
				if (c->get_optimize_scheduling() || (c->get_target_ABI() == Target_ABI::rtos)) {
					action_schedulingCondition_map[action_it->first].append("(" + sched_data_it->channel_name
						+ "_size >= " + std::to_string(sched_data_it->elements) + ")");
					action_post_exec_map[action_it->first].append("\t"+ sched_data_it->channel_name
						+ "_size -= " + std::to_string(sched_data_it->elements)+";\n");
				}
				else {
					std::string tmp;
					ABI_CHANNEL_SIZE(c, tmp, sched_data_it->channel_name)
					action_schedulingCondition_map[action_it->first].append("(" + tmp
						+ " >= " + std::to_string(sched_data_it->elements) + ")");
				}
			}
			else {
				if (!action_freeSpaceCondition_map[action_it->first].empty()) {
					action_freeSpaceCondition_map[action_it->first].append(" && ");
				}
				if (c->get_optimize_scheduling() || (c->get_target_ABI() == Target_ABI::rtos)) {
					action_freeSpaceCondition_map[action_it->first].append("(" + sched_data_it->channel_name
						+ "_free >= " + std::to_string(sched_data_it->elements) + ")");
					action_post_exec_map[action_it->first].append("\t"+ sched_data_it->channel_name
						+ "_free -= " + std::to_string(sched_data_it->elements) + ";\n");
				}
				else {
					std::string tmp;
					ABI_CHANNEL_FREE(c, tmp, sched_data_it->channel_name)
					action_freeSpaceCondition_map[action_it->first].append("(" + tmp
						+ " >= " + std::to_string(sched_data_it->elements) + ")");
				}
			}
		}
		if (action_schedulingCondition_map[action_it->first].empty()) {
			action_schedulingCondition_map[action_it->first] = "true";
		}
	}

	if (c->get_target_ABI() == Target_ABI::rtos) {
		return generate_rtos_scheduler(actions, fsm, priorities, converted_guard_conditions, action_schedulingCondition_map,
			                           action_freeSpaceCondition_map, state_channel_access, action_post_exec_map, schedule_function_name,
			                            scheduling_loop_bound, sched_loop, is_source, is_sink);
	}
	else if (c->get_sched_non_preemptive() || c->get_sched_rr()) {
		return default_local(actions, fsm, priorities,
			input_classification, output_classification, prefix,
			converted_guard_conditions,
			action_schedulingCondition_map,
			action_freeSpaceCondition_map,
			state_channel_access,
			action_post_exec_map,
			sched_loop,
			c->get_sched_rr(),
			schedule_function_name,
			schedule_function_parameter,
			no_else
			);
	}
	else {
		throw Scheduler_Generation_Exception{ "No Scheduling Strategy defined." };
	}
}



