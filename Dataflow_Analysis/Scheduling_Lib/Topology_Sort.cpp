#include "Scheduling_Lib.hpp"
#include <set>
#include <algorithm>
#include "Optimization_Phase2/Optimization_Phase2.hpp"

//find the starting point for the sort/search, ideally the actors without input buffers
//otherwise just pick an actor instance as it shouldn't matter if there are cycles in the network.
// This will just cause some initial overhead until the global scheduler advanced to the actual
// actor instances that have to be fired first to get the whole process going.
// The deleted flag is ignored as it doesn't matter, it is just a bit more computation time here.
// Actor composition: We rely on the fact the the network is not changed, only cluster ids are set in
// the actor instances. This way we can still find the init actors easily and avoid adding cycles by merging/clustering.
static void find_starting_point(
	std::vector<IR::Actor_Instance*>& actors,
	std::vector<IR::Actor_Instance*>& process_list,
	std::set<IR::Actor_Instance*>& unprocessed_list)
{

	// This would be sufficient if the networks always have nodes with only outgoing edges that are
	// the predecessors of all others, but it there are cycles and so on this is not always given.
	// Hence, we need this unprocessed list to keep track whether some nodes are not covered.
	for (auto it = actors.begin(); it != actors.end(); ++it) {
		if ((*it)->get_actor()->get_in_buffers().size() == 0) {
			unprocessed_list.erase(*it);
			process_list.push_back(*it);
		}
	}

	if (process_list.empty()) {
		// Found no actor instances that have no input, just pick one to start with
		// this might not be ideal, as cycle by cycle are processed leading to a mapping that might not be ideal
		process_list.push_back(actors[0]);
		unprocessed_list.erase(actors[0]);
	}
}

static bool all_predecessors_covered(
	IR::Actor_Instance* inst,
	std::vector<std::string>& sorted_actors)
{
	for (auto it = inst->get_in_edges().begin(); it != inst->get_in_edges().end(); ++it) {
		auto x = std::find(sorted_actors.begin(), sorted_actors.end(), (*it)->get_source()->get_name());
		if (x == sorted_actors.end()) {
			return false;
		}
	}
	return true;
}

void Scheduling::topology_sort(
	std::set<std::string>& actors,
	IR::Dataflow_Network* dpn,
	std::vector<std::string>& sorted_actors)
{
	std::vector<IR::Actor_Instance*> process_list;
	std::set<IR::Actor_Instance*> unprocessed_list; // This list excludes process_list!
	unprocessed_list.insert(dpn->get_actor_instances().begin(), dpn->get_actor_instances().end());
	if (dpn->get_inputs().empty()) {
		find_starting_point(dpn->get_actor_instances(), process_list, unprocessed_list);
	}
	else {
		for (auto t = dpn->get_inputs().begin(); t != dpn->get_inputs().end(); ++t) {
			unprocessed_list.erase(*t);
			process_list.push_back(*t);
		}
	}

	while (!process_list.empty()) {
		IR::Actor_Instance* actor = *process_list.begin();

		std::string name;
		if (actor->get_composit_actor() != nullptr) {
			name = actor->get_composit_actor()->get_name();
		}
		else {
			name = actor->get_name();
		}

		// If this actor is one of those that shall be sorted, add it to the list
		if (actors.find(name) != actors.end()) {
			//We can find a composit actor that is already in the sorted list
			//I think it is best to move it to the back of the sorted list as finding it
			//again means that it has an input edge that can only now deliver inputs but it
			//has been scheduled before already, might not work properly in case of cycles
			//but this cannot be excluded, too little information in the network
			auto x = std::find(sorted_actors.begin(), sorted_actors.end(), name);
			if (x != sorted_actors.end()) {
				sorted_actors.erase(x);
			}
			sorted_actors.push_back(name);
		}

		for (auto it = actor->get_out_edges().begin();
			it != actor->get_out_edges().end(); ++it)
		{
			if ((*it)->get_feedback()) {
				/* ignore feedback loops here */
				continue;
			}

			IR::Actor_Instance* a = (*it)->get_sink();

			if (all_predecessors_covered(a, sorted_actors) || process_list.empty()) {
				process_list.push_back(a);
			}
			if (unprocessed_list.find(a) != unprocessed_list.end()) {
				unprocessed_list.erase(a);
			}
		}

		process_list.erase(process_list.begin());

		if (process_list.empty() && !unprocessed_list.empty()) {
			// there is some part of the network that wasn't covered so far.
			// simply add some node of the unprocessed_list to the process_list and continue
			process_list.push_back(*unprocessed_list.begin());
			unprocessed_list.erase(unprocessed_list.begin());
		}
	}
}