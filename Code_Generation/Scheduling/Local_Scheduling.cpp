#include "Scheduling.hpp"
#include "Scheduling_Lib.hpp"
#include "Config/debug.h"
#include "Config/config.h"
#include <algorithm>
#include <tuple>
#include "String_Helper.h"

static std::string non_preemptive(
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
	std::map<std::string, std::string>& state_channel_access)
{
	std::string output{ };
	std::string local_prefix;
	bool static_rate = (input_classification != Actor_Classification::dynamic_rate);
	output.append(prefix + "void schedule(void) {\n");
	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "\tunsigned firings = 0;\n");
	output.append("#endif\n");
	output.append(prefix + "\tfor (;;) {\n");
	if (!fsm.empty()) {
		std::set<std::string> states = Scheduling::get_all_states(fsm);
		for (auto it = states.begin(); it != states.end(); ++it) {
			if (it == states.begin()) {
				output.append(prefix + "\t\tif (state == FSM::" + *it + ") {\n");
			}
			else {
				output.append(prefix + "\t\telse if (state == FSM::" + *it + ") {\n");
			}
			//find actions that could be scheduled in this state
			std::vector<std::string> schedulable_actions = find_schedulable_actions(*it, fsm, actions);
			//sort the list of schedulable actions with the comparsion function defined above if a priority block is defined
			if (!priorities.empty()) {
				std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
			}
			if (static_rate) {
				std::string channel_prefetch = state_channel_access[*it];
				replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t\t\t");
				output.append(prefix + "\t\t\tif " + action_schedulingCondition_map[schedulable_actions.front()] + " {\n");
				output.append(channel_prefetch);
				local_prefix = prefix + "\t\t\t\t";
			}
			else {
				local_prefix = prefix + "\t\t\t";
			}
			//create condition test and scheduling for each schedulable action
			for (auto action_it = schedulable_actions.begin();
				action_it != schedulable_actions.end(); ++action_it)
			{
				std::string action_condition = action_guard[*action_it];
				if (!static_rate) {
					action_condition.append(" && ");
					action_condition.append(action_schedulingCondition_map[*action_it]);
				}
				if (action_it == schedulable_actions.begin()) {
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
					output.append(local_prefix + "\tstate = FSM::" + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
				}
				else {
					output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*action_it] + ") {\n");
					output.append(local_prefix + "\t\t" + *action_it + "(" +
						get_action_in_parameters(*action_it, actions) + "); \n");
					output.append("#ifdef PRINT_FIRINGS\n");
					output.append(local_prefix + "\t\t++firings;\n");
					output.append("#endif\n");
					output.append(local_prefix + "\t\tstate = FSM::" + Scheduling::find_next_state(*it, *action_it, fsm) + ";\n");
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

			if (static_rate) {
				output.append(prefix + "\t\t\t}\n");
				output.append(prefix + "\t\t\telse {\n");
				output.append(prefix + "\t\t\t\tbreak;\n");
				output.append(prefix + "\t\t\t}\n");
			}

			output.append(prefix + "\t\t}\n");//close state checking if
		}
	}
	else {
		std::vector<std::string> schedulable_actions = find_schedulable_actions("", fsm, actions);
		if (!priorities.empty()) {
			std::sort(schedulable_actions.begin(), schedulable_actions.end(), Scheduling::comparison_object{ priorities });
		}
		if (static_rate) {
			// We only need to check size once
			output.append(prefix + "\t\tif " + action_schedulingCondition_map[schedulable_actions.front()] + " {\n");
			std::string channel_prefetch = state_channel_access[""];
			replace_all_substrings(channel_prefetch, "\t", prefix + "\t\t\t");
			output.append(channel_prefetch);
			local_prefix = prefix + "\t\t\t";
		}
		else {
			local_prefix = prefix + "\t\t";
		}
		for (auto it = schedulable_actions.begin(); it != schedulable_actions.end(); ++it) {
			std::string action_condition = action_guard[*it];
			if (!static_rate) {
				action_condition.append(" && ");
				action_condition.append(action_schedulingCondition_map[*it]);
			}
			if (it == schedulable_actions.begin()) {
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
			}
			else {
				output.append(local_prefix + "\tif (" + action_freeSpaceCondition_map[*it] + ") {\n");
				output.append(local_prefix + "\t\t" + *it + "(" + get_action_in_parameters(*it, actions) + "); \n");
				output.append("#ifdef PRINT_FIRINGS\n");
				output.append(local_prefix + "\t\t++firings;\n");
				output.append("#endif\n");
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
		if (static_rate) {
			output.append(prefix + "\t\t}\n");
			output.append(prefix + "\t\telse {\n");
			output.append(prefix + "\t\t\tbreak;\n");
			output.append(prefix + "\t\t}\n");
		}
	}
	output.append(prefix + "\t}\n");//close for(;;) loop
	output.append("#ifdef PRINT_FIRINGS\n");
	output.append(prefix + "\tstd::cout << actor$name << \" fired \" << firings << \" times\" << std::endl;\n");
	output.append("#endif\n");
	output.append(prefix + "}\n");// close scheduler method

	return output;
}

/* Replace channel variables used in the guards by either fetched local variables
 * or prefetch function calls.
 */
static std::string guard_var_replacement(
	std::string guard,
	std::map<std::string, std::string>& replacement_map)
{
	remove_whitespaces(guard);

	for (auto it = replacement_map.begin(); it != replacement_map.end(); ++it) {
		replace_all_substrings(guard, it->first, it->second);
	}

	return guard;
}

/* Update the guard conditions of the actions and generate code to fetch tokens from channels.
 * In the static cases (also cylco-static etc.) code to fetch the tokens and store it in local
 * variables is generated, the guard conditions are manipulated to use these local variables.
 * Otherwise the guard conditions are manipulated to use the prefetch functionality of the channel
 * to compute the guard condition.
 * The guards are adjusted in action_guard and the code to fetch the tokens is returned as
 * map, mapping the state to the fetch code. If no FSM is defined the code has the empty string as key.
 */
static std::map<std::string, std::string> get_scheduler_channel_access(
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data> >& actions,
	std::map<std::string, std::string>& action_guard,
	Actor_Classification input_classification,
	std::map<std::string, std::vector<std::string>>& actions_per_state)
{
	// Map the channel read to local variable for each state if required (not in the dynamic case)
	std::map<std::string, std::string> output;

	if (input_classification == Actor_Classification::dynamic_rate) {
		// It is dynamic, hence, we must prefetch
		for (auto action_it = actions.begin();
			action_it != actions.end(); ++action_it)
		{
			std::map<std::string, std::string> replacement_map;

			for (auto sched_data_it = action_it->second.begin();
				sched_data_it != action_it->second.end(); ++sched_data_it)
			{
				// One scheduling data entry per accessed channel
				unsigned index = 0;
				if (action_guard.contains(action_it->first) && (action_guard[action_it->first] != "")) {
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
								std::string n = sched_data_it->channel_name + "->preview(" + std::to_string(i * sched_data_it->var_names.size() + index) + ")";
								replacement_map[r] = n;
							}
						}
						else {
							std::string r = *var_it;
							std::string n = sched_data_it->channel_name + "->preview(" + std::to_string(index) + ")";
							replacement_map[r] = n;
						}
						++index;
					}
				}
			}
			std::string replaced_guard = guard_var_replacement(action_guard[action_it->first], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
			if ((replaced_guard != action_guard[action_it->first]) && !replaced_guard.empty()) {
				std::cout << "Action: " << action_it->first << " Replace guard " << action_guard[action_it->first] << " by " << replaced_guard << std::endl;
			}
#endif
			action_guard[action_it->first] = replaced_guard;
		}
	}
	else {
		//Must be some classification that demands that all actions consume the same number of tokens
		//Hence, we can load the channel data to a local variable, evaluate guards and the forward the
		//local variable to the action
		if (!actions_per_state.empty()) {
			for (auto state_it = actions_per_state.begin(); state_it != actions_per_state.end(); ++state_it) {
				for (auto action_it = state_it->second.begin(); action_it != state_it->second.end(); ++action_it) {
					std::map<std::string, std::string> replacement_map;

					for (auto sched_data_it = actions[*action_it].begin();
						sched_data_it != actions[*action_it].end(); ++sched_data_it)
					{
						// One scheduling data entry per accessed channel
						unsigned index = 0;
						if (action_guard.contains(*action_it) && (action_guard[*action_it] != "")) {
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
										std::string n = sched_data_it->channel_name + "$param[" + std::to_string(i * sched_data_it->var_names.size() + index) + "]";
										replacement_map[r] = n;
									}
								}
								else {
									if (sched_data_it->elements == 1) {
										std::string r = *var_it;
										std::string n = sched_data_it->channel_name + "$param";
										replacement_map[r] = n;
									}
									else {
										std::string r = *var_it;
										std::string n = sched_data_it->channel_name + "$param[" + std::to_string(index) + "]";
										replacement_map[r] = n;
									}
								}
								++index;
							}
						}
					}
					std::string replaced_guard = guard_var_replacement(action_guard[*action_it], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
					if ((replaced_guard != action_guard[*action_it]) && !replaced_guard.empty()) {
						std::cout << "Action: " << *action_it << " Replace guard " << action_guard[*action_it] << " by " << replaced_guard << std::endl;
					}
#endif
					action_guard[*action_it] = replaced_guard;
				}

				std::string local_def;
				for (auto it = actions[state_it->second.front()].begin(); it != actions[state_it->second.front()].end(); ++it) {
					if (it->unused_channel || !it->in) {
						continue;
					}
					if (it->elements == 1) {
						local_def.append("\t" + it->type + " " + it->channel_name + "$param = " + it->channel_name + "->read();\n");
					}
					else {
						local_def.append("\t" + it->type + " " + it->channel_name + "$param[" + std::to_string(it->elements) + "];\n");
						local_def.append("\tfor (unsigned i = 0; i < " + std::to_string(it->elements) + "; ++i) {" + it->channel_name + "$param[i] = " + it->channel_name + "->read();}\n");
					}
				}
#ifdef DEBUG_SCHEDULER_GENERATION
				std::cout << "Channel prefetch code for state " << state_it->first << ":" << local_def << std::endl;
#endif
				output[state_it->first] = local_def;
			}
		}
		else {
			// No FSM, hence, it is the static case
			for (auto action_it = actions.begin();
				action_it != actions.end(); ++action_it)
			{
				std::map<std::string, std::string> replacement_map;

				for (auto sched_data_it = action_it->second.begin();
					sched_data_it != action_it->second.end(); ++sched_data_it)
				{
					// One scheduling data entry per accessed channel
					unsigned index = 0;
					if (action_guard.contains(action_it->first) && (action_guard[action_it->first] != "")) {
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
									std::string n = sched_data_it->channel_name + "$param[" + std::to_string(i * sched_data_it->var_names.size() + index) + "]";
									replacement_map[r] = n;
								}
							}
							else {
								if (sched_data_it->elements == 1) {
									std::string r = *var_it;
									std::string n = sched_data_it->channel_name + "$param";
									replacement_map[r] = n;
								}
								else {
									std::string r = *var_it;
									std::string n = sched_data_it->channel_name + "$param[" + std::to_string(index) + "]";
									replacement_map[r] = n;
								}
							}
							++index;
						}
					}
				}
				std::string replaced_guard = guard_var_replacement(action_guard[action_it->first], replacement_map);
#ifdef DEBUG_SCHEDULER_GENERATION
				if (!replaced_guard.empty() && (action_guard[action_it->first] != replaced_guard)) {
					std::cout << "Action: " << action_it->first << " Replace guard " << action_guard[action_it->first] << " by " << replaced_guard << std::endl;
				}
#endif
				action_guard[action_it->first] = replaced_guard;
			}
			std::string local_def;
			for (auto it = actions.begin()->second.begin(); it != actions.begin()->second.end(); ++it) {
				if (it->unused_channel || !it->in) {
					continue;
				}
				if (it->elements == 1) {
					local_def.append("\t" + it->type + " " + it->channel_name + "$param = " + it->channel_name + "->read();\n");
				}
				else {
					local_def.append("\t" + it->type + " " + it->channel_name + "$param[" + std::to_string(it->elements) + "];\n");
					local_def.append("\tfor (unsigned i = 0; i < " + std::to_string(it->elements) + "; ++i) {" + it->channel_name + "$param[i] = " + it->channel_name + "->read();}\n");
				}
			}
#ifdef DEBUG_SCHEDULER_GENERATION
			std::cout << "Channel prefetch code:" << local_def << std::endl;
#endif
			output[""] = local_def;
		}
	}

	return output;
}


std::string Scheduling::generate_local_scheduler(
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data> >& actions,
	std::map<std::string, std::string>& action_guard,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	Actor_Classification input_classification,
	Actor_Classification output_classification,
	std::string prefix)
{
	Config* c = c->getInstance();

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

	std::map<std::string, std::vector<std::string>> actions_per_state;

	for (auto it : get_all_states(fsm)) {
		std::vector<std::string> sched_actions = find_schedulable_actions(it, fsm, actions);
		actions_per_state[it] = sched_actions;
	}

	std::map<std::string, std::string> state_channel_access = 
		get_scheduler_channel_access(actions, action_guard, input_classification, actions_per_state);

	// Set guard to (true) if no guard is specified as this avoids checking during code generation
	// whether a guard is used or not. The C-Compiler can optimize this.
	for (auto it = action_guard.begin(); it != action_guard.end(); ++it) {
		if (it->second.empty()) {
			it->second = "(true)";
		}
		else {
			it->second = "(" + it->second + ")";
		}
	}

	//init with empty first so we don't need to care later and keep the guarantee that every action is in there
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		action_schedulingCondition_map[it->first] = "";
		action_freeSpaceCondition_map[it->first] = "";
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
				action_schedulingCondition_map[action_it->first].append("(" + sched_data_it->channel_name
					+ "->size() >= " + std::to_string(sched_data_it->elements) + ")");
			}
			else {
				if (!action_freeSpaceCondition_map[action_it->first].empty()) {
					action_freeSpaceCondition_map[action_it->first].append(" && ");
				}
				action_freeSpaceCondition_map[action_it->first].append("(" + sched_data_it->channel_name
					+ "->free() >= " + std::to_string(sched_data_it->elements) + ")");
			}
		}
		if (action_schedulingCondition_map[action_it->first].empty()) {
			action_schedulingCondition_map[action_it->first] = "(true)";
		}
	}

	if (c->get_non_preemptive()) {
		return non_preemptive(actions, fsm, priorities,
			input_classification, output_classification, prefix,
			action_guard,
			action_schedulingCondition_map,
			action_freeSpaceCondition_map,
			state_channel_access);
	}
	else {
		throw Scheduler_Generation_Exception{ "No Scheduling Strategy defined." };
	}
}



