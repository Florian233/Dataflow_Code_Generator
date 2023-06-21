#include "IR/actor.hpp"
#include <vector>
#include <tuple>
#include <map>
#include <algorithm>
#include "Actor_Classification.hpp"
#include "Config/debug.h"

// return true if the merge was without conflicts, false it there was a conflict while merging
static bool merge_maps(
	std::map<std::string, unsigned>& map1,
	std::map<std::string, unsigned>& map2)
{
	bool ret = true;
	for (auto it = map2.begin(); it != map2.end(); ++it) {
		if (map1.contains(it->first)) {
			if (map1[it->first] != it->second) {
				ret = false;
			}
		}
		else {
			map1[it->first] = it->second;
		}
	}
	return ret;
}

/* Compare two maps and return true if map2 is a subset of map1, false otherwise. */
static bool compare_maps(
	std::map<std::string, unsigned>& map1,
	std::map<std::string, unsigned>& map2)
{
	for (auto it = map1.begin(); it != map1.end(); ++it) {
		if (!map2.contains(it->first) || map2[it->first] != it->second) {
			return false;
		}
	}
	return true;
}

// the FSM might only use an action tag and not the complete name,
// but this classification requires the complete names to find the tokenrates
// Hence, translate the action in a FSM entry (might be a tag) to a list of actions that
// can be scheduled in this state.
static std::vector<std::string> find_actions(
	std::string action_fsm,
	std::vector<IR::Action*>& actions)
{
	std::vector<std::string> ret;
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		std::string name = (*it)->get_name();
		if (action_fsm == name) {
			ret.push_back(name);
		}
		else if (name.compare(0, action_fsm.size(), action_fsm) == 0) {
			if (name[action_fsm.size()] == '.') {
				ret.push_back(name);
			}
		}
	}
	return ret;
}

#ifdef DEBUG_CLASSIFICATION
static void print_classification(Actor_Classification c) {
	if (c == Actor_Classification::static_rate) {
		std::cout << "Static";
	}
	else if (c == Actor_Classification::cyclostatic_rate) {
		std::cout << "Cyclo-static";
	}
	else if (c == Actor_Classification::singlecycle_cyclostatic_rate) {
		std::cout << "Cyclo-static with single cycle FSM";
	}
	else if (c == Actor_Classification::state_static) {
		std::cout << "Static-rate per state";
	}
	else {
		std::cout << "Dynamic";
	}
}

static void print_actor_classification(
	Actor_Classification in,
	Actor_Classification out,
	std::string name)
{
	std::cout << "Actor " << name << " classified as: Input: ";
	print_classification(in);
	std::cout << " Output: ";
	print_classification(out);
	std::cout << std::endl;
}

#endif

void IR::Actor::classify_actor(void) {
	if (fsm.size() == 0) {
		bool static_in{ true };
		bool static_out{ true };
		//check static first, this is the most restrictive, then cyclo static,
		// dynamic is not checked as it is the default
		// but it cannot be static if it has a FSM
		std::map<std::string, unsigned> channel_rate_map;
		for (auto it = actions.begin(); it != actions.end(); ++it) {
			if ((*it)->is_init()) {
				continue;
			}
			for (auto c_it = (*it)->get_in_buffers().begin();
				c_it != (*it)->get_in_buffers().end(); ++c_it)
			{
				if (channel_rate_map.contains(c_it->buffer_name)
					&& channel_rate_map[c_it->buffer_name] != c_it->tokenrate)
				{
					static_in = false;
					break; //not static input
				}
				else {
					channel_rate_map[c_it->buffer_name] = c_it->tokenrate;
				}
			}
			if (!static_in) {
				break;
			}
		}
		channel_rate_map.clear();
		for (auto it = actions.begin(); it != actions.end(); ++it) {
			if ((*it)->is_init()) {
				continue;
			}
			for (auto c_it = (*it)->get_out_buffers().begin();
				c_it != (*it)->get_out_buffers().end(); ++c_it)
			{
				if (channel_rate_map.contains(c_it->buffer_name)
					&& channel_rate_map[c_it->buffer_name] != c_it->tokenrate)
				{
					static_out = false;
					break; //not static output
				}
				else {
					channel_rate_map[c_it->buffer_name] = c_it->tokenrate;
				}
			}
			if (!static_out) {
				break;
			}
		}
		if (static_in) {
			input_classification = Actor_Classification::static_rate;
		}
		if (static_out) {
			output_classification = Actor_Classification::static_rate;
		}

#ifdef DEBUG_CLASSIFICATION
		print_actor_classification(input_classification, output_classification, actor_name);
#endif
		return;
	}

	//Create a map with (action name -> (channel name -> rate)) for input and output
	std::map<std::string, std::map<std::string, unsigned>> channel_rate_map_in;
	std::map<std::string, std::map<std::string, unsigned>> channel_rate_map_out;
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		std::map<std::string, unsigned> in;
		std::map<std::string, unsigned> out;

		if ((*it)->is_init()) {
			continue;
		}
		for (auto c_it = (*it)->get_in_buffers().begin();
			c_it != (*it)->get_in_buffers().end(); ++c_it)
		{
			in[c_it->buffer_name] = c_it->tokenrate;
		}
		for (auto c_it = (*it)->get_out_buffers().begin();
			c_it != (*it)->get_out_buffers().end(); ++c_it)
		{
			out[c_it->buffer_name] = c_it->tokenrate;
		}
		channel_rate_map_in[(*it)->get_name()] = in;
		channel_rate_map_out[(*it)->get_name()] = out;
	}

	std::map<std::string, std::vector<std::string>> state_actions_map;
	std::map<std::string, std::string> single_cycle_map;
	std::map<std::string, std::vector<std::string>> state_next_map;

	bool has_single_cyle{ true };

	for (unsigned i = 0; i < fsm.size(); ++i) {
		FSM_Entry e = fsm[i];

		if (state_actions_map.contains(e.state)) {
			auto x = find_actions(e.action, actions);
			state_actions_map[e.state].insert(state_actions_map[e.state].end(), x.begin(), x.end());
		}
		else {
			std::vector<std::string> v{ find_actions(e.action, actions) };
			state_actions_map[e.state] = v;
		}

		if (state_next_map.contains(e.state)) {
			state_next_map[e.state].push_back(e.next_state);
		}
		else {
			std::vector<std::string> v{ e.next_state };
			state_next_map[e.state] = v;
		}

		if (single_cycle_map.contains(e.state)) {
			if (single_cycle_map[e.state] != e.next_state) {
				has_single_cyle = false;
			}
		}
		else {
			single_cycle_map[e.state] = e.next_state;
		}
	}

#if 0
	for (auto it = state_actions_map.begin(); it != state_actions_map.end(); ++it) {
		std::cout << "State: " << it->first << " schedulable actions: ";
		for (auto ait = it->second.begin(); ait != it->second.end(); ++ait) {
			std::cout << *ait << " ";
		}
		std::cout << std::endl;
	}
#endif

	/* find all paths through the FSM that start with the initial state,
	   a path ends when a state is reached that is already included in the path
	   if it is not the initial state, there are other cycles in the fsm that are unexpected,
	   maybe initial state is more of an initialization part
	 */
	std::vector<std::vector<std::string>> fsm_paths;
	std::multimap<std::string, std::vector<std::string>> processed_paths;
	processed_paths.emplace(initial_state, std::vector<std::string>());

	while (!processed_paths.empty()) {
		auto e = processed_paths.begin();
		std::string state = e->first;
		std::vector<std::string> l = e->second;
		processed_paths.erase(e);
		if (!state_next_map.contains(state) || state_next_map[state].size() == 0) {
			/* there is no next state for this state, this is an one - way street,
			   hence, there cannot be a cycle!
			   This doesn't mean that this is the case for the complete FSM,
			   but it has a cycle that doesn't end with the initial state
			 */
#ifdef DEBUG_CLASSIFICATION
			print_actor_classification(input_classification, output_classification, actor_name);
#endif
			return;
		}
		for (auto it = state_next_map[state].begin(); it != state_next_map[state].end(); ++it) {
			std::vector<std::string> local{ l };
			local.push_back(state);
			auto found = std::find(local.begin(), local.end(), *it);
			if (found != local.end()) {
				// found -> path comes to an end with this state
				local.push_back(*it);
				fsm_paths.push_back(local);
			}
			else {
				//not found
				processed_paths.insert({ *it, local });
			}
		}
	}

#if 0
	for (auto it = fsm_paths.begin(); it != fsm_paths.end(); ++it) {
		std::cout << "Path:";
		for (auto i = it->begin(); i != it->end(); ++i) {
			std::cout << " " << *i;
		}
		std::cout << std::endl;
	}
#endif

	// now compute the tokenrates for each state
	std::map<std::string, std::map<std::string, unsigned>> state_tokenrate_map_in;
	std::map<std::string, std::map<std::string, unsigned>> state_tokenrate_map_out;

	bool cyclostatic_in{ true }; // Set to true, to signal non-conformity by setting it to false
	bool cyclostatic_out{ true };

	for (auto it = state_actions_map.begin(); it != state_actions_map.end(); ++it) {
		std::map<std::string, unsigned> in;
		std::map<std::string, unsigned> out;
		for (auto ait = it->second.begin(); ait != it->second.end(); ++ait) {
			if (!channel_rate_map_in.contains(*ait) || !channel_rate_map_out.contains(*ait)) {
				std::cout << "FSM processing failed for actor classification, actions in FSM detected that don't exit" << std::endl;
				exit(5);
			}
			bool conflict_in = merge_maps(in, channel_rate_map_in[*ait]);
			bool conflict_out = merge_maps(out, channel_rate_map_out[*ait]);
			cyclostatic_in &= conflict_in; //will stay true if no conflict was detected
			cyclostatic_out &= conflict_out;
		}
		state_tokenrate_map_in[it->first] = in;
		state_tokenrate_map_out[it->first] = out;
	}

	if (!cyclostatic_in && !cyclostatic_out) {
		//classification not fulfilled already, bail out
#ifdef DEBUG_CLASSIFICATION
		print_actor_classification(input_classification, output_classification, actor_name);
#endif
		return;
	}

	// so far we checked that all states have static rates, this can be reused if cyclo static things fail
	bool state_static_in = cyclostatic_in;
	bool state_static_out = cyclostatic_out;

	// Check if all paths are of the same length and end in the initial state
	{
		size_t previous_size = fsm_paths[0].size();
		for (auto it = fsm_paths.begin(); it != fsm_paths.end(); ++it) {
			if (it->size() != previous_size || it->back() != initial_state) {
				cyclostatic_in = false;
				cyclostatic_out = false;
			}
			else {
				previous_size = it->size();
			}
		}
	}

	if (cyclostatic_in || cyclostatic_out) {
		size_t path_length = fsm_paths[0].size();
		for (size_t i = 0; i < path_length; ++i) {
			//now check that in each path the same tokenrates are used for each step
			std::map<std::string, unsigned> in;
			std::map<std::string, unsigned> out;
			for (size_t j = 1; j < fsm_paths.size(); ++j) {
				cyclostatic_in &= compare_maps(state_tokenrate_map_in[fsm_paths[j - 1][i]],
					state_tokenrate_map_in[fsm_paths[j][i]]);
				cyclostatic_out &= compare_maps(state_tokenrate_map_out[fsm_paths[j - 1][i]],
					state_tokenrate_map_out[fsm_paths[j][i]]);
			}
		}
	}
	
	if (has_single_cyle) {
		if (cyclostatic_in) {
			input_classification = Actor_Classification::singlecycle_cyclostatic_rate;
		}
		if (cyclostatic_out) {
			output_classification = Actor_Classification::singlecycle_cyclostatic_rate;
		}
	}
	else {
		if (cyclostatic_in) {
			input_classification = Actor_Classification::cyclostatic_rate;
		}
		else if (state_static_in) {
			input_classification = Actor_Classification::state_static;
		}
		if (cyclostatic_out) {
			output_classification = Actor_Classification::cyclostatic_rate;
		}
		else if (state_static_out) {
			output_classification = Actor_Classification::state_static;
		}
	}
#ifdef DEBUG_CLASSIFICATION
	print_actor_classification(input_classification, output_classification, actor_name);
#endif
}
