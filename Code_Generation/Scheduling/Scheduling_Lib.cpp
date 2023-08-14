#include "Scheduling_Lib.hpp"
#include <algorithm>

void Scheduling::find_actors_for_core(
	unsigned core,
	IR::Dataflow_Network* dpn,
	std::set<std::string>& list)
{
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->is_deleted()) {
			continue;
		}
		if ((*it)->get_mapping() == core) {
			if ((*it)->get_composit_actor() != nullptr) {
				list.insert((*it)->get_composit_actor()->get_name());
			}
			else {
				list.insert((*it)->get_name());
			}
		}
	}
}

/*
	Finds all actions that are schedulable in the given state and returns their methodnames
*/
std::vector<std::string> Scheduling::find_schedulable_actions(
	std::string state,
	std::vector<IR::FSM_Entry>& fsm,
	std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data > >& actions)
{
	std::vector<std::string> output;

	if (state == "") {
		for (auto it = actions.begin(); it != actions.end(); ++it) {
			output.push_back(it->first);
		}
	}
	else {
		//go through all fsm entries and find the ones for the given state
		for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
			if (fsm_it->state == state) {
				//fsm_it->action might only be the tag, hence, we have to compare with the actual action names
				for (auto it = actions.begin(); it != actions.end(); ++it) {
					if ((fsm_it->action == it->first)
						|| ((it->first.find(fsm_it->action) == 0)
							&& (it->first[fsm_it->action.size()] == '$')))
					{
						output.push_back(it->first);
					}
				}
			}
		}
	}
	return output;
}

/*
	Finds for a given state and the action that has been fired in this state the corresponding next state according to the transitions of the FSM.
	Throws an exception if it cannot find the next state.
*/
std::string Scheduling::find_next_state(
	std::string current_state,
	std::string fired_action,
	std::vector<IR::FSM_Entry>& fsm)
{
	for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
		if ((fsm_it->state == current_state)
			&& ((fsm_it->action == fired_action)
				|| ((fired_action.find(fsm_it->action) == 0) && (fired_action[fsm_it->action.size()] == '$'))))
		{
			return fsm_it->next_state;
		}

	}

	//cannot find the next state
	throw Scheduling::Scheduler_Generation_Exception{ "Cannot find the next state. Current state:" + current_state + ", Fired Action:" + fired_action };
}

std::set<std::string> Scheduling::get_all_states(
	std::vector<IR::FSM_Entry>& fsm)
{
	std::set<std::string> result;
	for (auto fsm_it = fsm.begin(); fsm_it != fsm.end(); ++fsm_it) {
		result.insert(fsm_it->state);
		result.insert(fsm_it->next_state);
	}
	if (result.empty()) {
		//indicate that we don't have states, this "value" can be forwarded to other services directly,
		//the caller doesn't need to care whether this set is empty or not
		result.insert("");
	}
	return result;
}

std::string Scheduling::get_action_in_parameters(
	std::string action,
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions)
{
	std::string output;

	for (auto it = actions[action].begin(); it != actions[action].end(); ++it) {
		if (!it->in || it->unused_channel || !it->parameter_generated) {
			continue;
		}
		if (!output.empty()) {
			output.append(", ");
		}
		output.append(it->channel_name + "$param");
	}

	return output;
}

class Sched_Order_Sort {
	std::map<std::string, int>& sched_order_map;
public:
	Sched_Order_Sort(std::map<std::string, int>& a) : sched_order_map{ a } {};

	bool operator()(std::string a, std::string b) const {
		// Actor instances with large levels first in the list
		return sched_order_map[a] > sched_order_map[b];
	}
};

bool Scheduling::sched_order_sort(
	std::set<std::string>& actors,
	IR::Dataflow_Network* dpn,
	std::vector<std::string>& sorted_actors)
{
	std::map<std::string, int> sched_order_map;

	for (auto it = dpn->get_actor_instances().begin(); it != dpn->get_actor_instances().end(); ++it) {
		if ((*it)->get_sched_order() == -1) {
			return false;
		}
		sched_order_map[(*it)->get_name()] = (*it)->get_sched_order();
	}

	std::vector<std::string> tmp(actors.begin(), actors.end());

	std::sort(tmp.begin(), tmp.end(), Sched_Order_Sort{ sched_order_map });

	sorted_actors.swap(tmp);

	return true;
}