#pragma once

#include "Scheduling.hpp"

namespace Scheduling {

	/* Sort Actor Instances in actors accordingly to the network specified in dpn.
	 * Function returns the result in sorted_actors parameter.
	 */
	void topology_sort(
		std::set<std::string>& actors,
		IR::Dataflow_Network* dpn,
		std::vector<std::string>& sorted_actors);

	/* Find all actor instances that are assigned to a specific core. 
	 * The function returns the result in list.
	 */
	void find_actors_for_core(
		unsigned core,
		IR::Dataflow_Network* dpn,
		std::set<std::string>& list);

	/*
		Finds all actions that are schedulable in the given state and returns their methodnames
	*/
	std::vector<std::string> find_schedulable_actions(
		std::string state,
		std::vector<IR::FSM_Entry>& fsm,
		std::map<std::string, std::vector< Scheduling::Channel_Schedule_Data > >& actions);

	/*
		Finds for a given state and the action that has been fired in this state the corresponding next state according to the transitions of the FSM.
		Throws an exception if it cannot find the next state.
	*/
	std::string find_next_state(
		std::string current_state,
		std::string fired_action,
		std::vector<IR::FSM_Entry>& fsm);

	/* Get a list of all FSM states. */
	std::set<std::string> get_all_states(
		std::vector<IR::FSM_Entry>& fsm);

	/* Get a string that contains the parameters of an action call.
	 * If input channel tokens are fetched by the local scheduler to compute guards
	 * and the behaviour is static no prefetch is used. Hence, the data has to be passed to
	 * the function. The string for the function call parameteres is determined by this function.
	 */
	std::string get_action_in_parameters(
		std::string action,
		std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions);

	/*
		Function object to sort actions according to their priority defined in the priority block of the actor.
	*/
	class comparison_object {
		//(Tag,Name) > (Tag,Name) pairs 
		std::vector<IR::Priority_Entry>& priorities;

	public:
		comparison_object(std::vector<IR::Priority_Entry>& _priorities) :priorities(_priorities) {}

		//strings are method names
		bool operator()(std::string& str1, std::string& str2) const {
			for (auto it = priorities.begin(); it != priorities.end(); ++it) {
				//str1 > str2
				if (str1 == it->action_high) {
					if ((str2 == it->action_low)
						|| ((str2.find(it->action_low) == 0)
							&& (str2[it->action_low.size() == '$'])))
					{
						return true;
					}
				}
				//str2 > str1
				else if (str2 == it->action_high) {
					if ((str1 == it->action_low)
						|| ((str1.find(it->action_low) == 0)
							&& (str1[it->action_low.size()] == '$')))
					{
						return false;
					}
				}
				//str1 > str2
				else if ((str1.find(it->action_high) == 0)
					&& (str1[it->action_high.size()] == '$'))
				{
					if ((str2 == it->action_low)
						|| ((str2.find(it->action_low) == 0)
							&& (str2[it->action_low.size() == '$'])))
					{
						return true;
					}
				}
				//str2 > str1
				else if ((str2.find(it->action_high) == 0)
					&& (str2[it->action_high.size() == '$']))
				{
					if ((str1 == it->action_low)
						|| ((str1.find(it->action_low) == 0)
							&& (str1[it->action_low.size()] == '$')))
					{
						return false;
					}
				}
			}
			return true;
		}
	};

	/* Sort Actor Instances in actors accordingly to the scheduling order specified in the actor instances.
	 * Function returns the result in sorted_actors parameter and returns true if sched_order sorting was possible.
	 */
	bool sched_order_sort(
			std::set<std::string>& actors,
			IR::Dataflow_Network *dpn,
			std::vector<std::string>& sorted_actors);
}