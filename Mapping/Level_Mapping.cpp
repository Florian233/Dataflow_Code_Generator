#include "Mapping.hpp"
#include "Mapping_Lib.hpp"
#include "Config/config.h"
#include <algorithm>
#include <vector>

static void lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map)
{
	Config* c = c->getInstance();
	unsigned core = 0;
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	
	// Now distribute them in round-robin fashion between the cores
	for (auto it = sorted_actors.begin(); it != sorted_actors.end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		(*it)->set_mapping(core);
		(*it)->set_sched_order(actor_level_map[*it]);
		++core;
		core = core % c->get_cores();
	}
}

static void lft_horizontal_partitioning(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	unsigned max_level)
{
	Config* c = c->getInstance();
	unsigned range_per_core = (max_level / c->get_cores()) + 1;

	for (auto it = actor_level_map.begin(); it != actor_level_map.end(); ++it) {
		unsigned core = it->second / range_per_core;
		core = core % c->get_cores();
		it->first->set_mapping(core);
		it->first->set_sched_order(actor_level_map[it->first]);
	}
}

struct core_balance {
	unsigned core = 0;
	unsigned weight = 0;
	unsigned end = UINT32_MAX;
	std::vector<IR::Actor_Instance*> actors;
};

static core_balance* get_core_least_weight(
	std::vector<core_balance>& cb)
{
	core_balance* b = &cb.at(0);
	for (auto cb_it = (++cb.begin()); cb_it != cb.end(); ++cb_it) {
		if (cb_it->weight < b->weight) {
			b = &(*cb_it);
		}
	}
	return b;
}

static IR::Actor_Instance* find_unmapped_next_level(
	std::vector<IR::Actor_Instance*>& sorted_actors,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	unsigned end)
{
	Config* c = c->getInstance();

	for (auto it = sorted_actors.begin(); it != sorted_actors.end(); ++it) {
		if (((*it)->get_mapping() >= c->get_cores()) && (actor_level_map[*it] <= end)
			&& !(*it)->is_deleted())
		{
			return *it;
		}
	}
	return nullptr;
}

static IR::Actor_Instance* get_unmapped_actor_highest_level(
	std::vector<IR::Actor_Instance*>& sorted_actor_instances)
{
	Config* c = c->getInstance();
	for (auto it = sorted_actor_instances.begin(); it != sorted_actor_instances.end(); ++it) {
		if ((*it)->get_mapping() > c->get_cores() && !(*it)->is_deleted()) {
			return *it;
		}
	}
	return nullptr;
}

static void simple_balanced_lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	Config* c = c->getInstance();
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	std::vector<core_balance> cb;

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		core_balance b;
		b.core = core;
		b.weight = 0;
		b.end = 0;
		cb.push_back(b);
	}

	for (auto it = sorted_actors.begin(); it != sorted_actors.end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		//Find the core with the least weight and assign the actor instance there
		core_balance* b = get_core_least_weight(cb);

		(*it)->set_mapping(b->core);
		(*it)->set_sched_order(actor_level_map[*it]);
		b->weight += actor_weight_map[*it];
	}
}

static void balanced_lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	Config* c = c->getInstance();
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	std::vector<core_balance> cb;

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		core_balance b;
		b.core = core;
		b.weight = 0;
		b.end = UINT32_MAX;
		cb.push_back(b);
	}

	for (auto it = sorted_actors.begin(); it != sorted_actors.end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		unsigned level = actor_level_map[*it];
		unsigned weight = actor_weight_map[*it];
		//Find the core with the least weight with end >= level von it and assign the actor instance there
		//Find a core where the end is larger but as close as possible, it two are equal use the overall weight of the core
		//Overlapping is avoided here (might also be possible that a overlap of x% is okay), also no balance formular between
		//distance between end of the core and level and the weight assigned to the core
		//If no core with an end >= level can be found assign to the core that is closest
		core_balance* b = &cb.at(0);
		for (auto cb_it = (++cb.begin()); cb_it != cb.end(); ++cb_it) {
			if (b->end > level) {
				//Both have an end before this actor shall start
				if (cb_it->end > level) {
					//Pick by weight
					if (b->weight > cb_it->weight) {
						b = &(*cb_it);
					}
					else {
						// b stays as is
					}
				}
				else {
					// b stays as is
				}
				b->end = level + weight;
			}
			else {
				//b ends after this actor shall start, if this is not the case of cb_it, change b
				if (cb_it->end > level) {
					b = &(*cb_it);
					b->end = level + weight;
				}
				else {
					// Try to minimize overlapping, if both overlap completely, pick the one with the least weight
					//Otherwise pick the one that ends earliest to minimize overlapping and give the following
					//actor instances a change to find something
					if ((level + weight) < b->end) {
						if (b->end < cb_it->end) {
							b = &(*cb_it);
						}
						else {
							// b stays as is
						}
						b->end = level + weight;
					}
					else if ((level + weight) < cb_it->end) {
						//not smaller than b.end
						b = &(*cb_it);
						b->end = level + weight;
					}
					else {
						// both overlap, pick the one with the least weight
						if (cb_it->weight < b->weight) {
							b = &(*cb_it);
						}
						else {
							// b stays as is
						}
						//no change of end, as this completely overlaps
					}
				}
			}
		}

		(*it)->set_mapping(b->core);
		(*it)->set_sched_order(actor_level_map[*it]);
		b->weight += actor_weight_map[*it];
	}
}

static unsigned count_unmapped_predecessors(
	IR::Actor_Instance* inst,
	unsigned num_cores)
{
	unsigned result = 0;
	for (auto it = inst->get_in_edges().begin(); it != inst->get_in_edges().end(); ++it) {
		if ((*it)->get_source()->get_mapping() >= num_cores) {
			++result;
		}
	}
	return result;
}

static IR::Actor_Instance* find_closest_least_predecessors(
	IR::Actor_Instance* inst,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map)
{
	Config* c = c->getInstance();
	IR::Actor_Instance* ret = nullptr;
	size_t val = 0;
	for (auto it = inst->get_out_edges().begin(); it != inst->get_out_edges().end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		if ((*it)->get_sink()->get_mapping() < c->get_cores() || (*it)->get_sink()->is_deleted()) {
			continue;
		}
		if (ret == nullptr) {
			ret = (*it)->get_sink();
			val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
		}
		else {
			size_t tmp_val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
			if (tmp_val < val) {
				val = tmp_val;
				ret = (*it)->get_sink();
			}
		}
	}

	return ret;
}

/* Criteria: 1. smalles gap in LFT schedule, 2. lowest number of predecessors */
static IR::Actor_Instance* find_closest(
	IR::Actor_Instance* inst,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map)
{
	Config* c = c->getInstance();
	IR::Actor_Instance* ret = nullptr;
	unsigned ret_level = 0;
	size_t val = 0;
	for (auto it = inst->get_out_edges().begin(); it != inst->get_out_edges().end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		if ((*it)->get_sink()->get_mapping() < c->get_cores() || (*it)->get_sink()->is_deleted()) {
			continue;
		}
		if ((ret == nullptr) || (actor_level_map[(*it)->get_sink()] > ret_level)) {
			ret = (*it)->get_sink();
			ret_level = actor_level_map[ret];
			val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
		}
		else if (actor_level_map[(*it)->get_sink()] == ret_level) {
			size_t tmp_val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
			if (tmp_val < val) {
				val = tmp_val;
				ret = (*it)->get_sink();
			}
		}
	}

	return ret;
}

static IR::Actor_Instance* find_closest_with_limit(
	IR::Actor_Instance* inst,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	unsigned weight_limit,
	std::map<IR::Actor_Instance*, unsigned>& weight_map)
{
	Config* c = c->getInstance();
	IR::Actor_Instance* ret = nullptr;
	unsigned ret_level = 0;
	size_t val = 0;
	for (auto it = inst->get_out_edges().begin(); it != inst->get_out_edges().end(); ++it) {
		if ((*it)->is_deleted()) {
			continue;
		}
		if ((*it)->get_sink()->get_mapping() < c->get_cores() || (*it)->get_sink()->is_deleted()) {
			continue;
		}
		if (weight_map[(*it)->get_sink()] > weight_limit) {
			continue;
		}
		if ((ret == nullptr) || (actor_level_map[(*it)->get_sink()] > ret_level)) {
			ret = (*it)->get_sink();
			ret_level = actor_level_map[ret];
			val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
		}
		else if (actor_level_map[(*it)->get_sink()] == ret_level) {
			size_t tmp_val = count_unmapped_predecessors((*it)->get_sink(), c->get_cores());
			if (tmp_val < val) {
				val = tmp_val;
				ret = (*it)->get_sink();
			}
		}
	}

	return ret;
}

#if 0
static inline unsigned fits_gap(
	IR::Actor_Instance* inst,
	core_balance& balance,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map)
{
	unsigned level = actor_level_map[inst];
	unsigned weight = actor_weight_map[inst];

	for (auto it = balance.actors.begin(); it != balance.actors.end(); ++it) {
		if ()
	}
}
#endif

static IR::Actor_Instance* find_next(
	IR::Actor_Instance* inst,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map_lft,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	std::vector<IR::Actor_Instance*>& sorted_actors)
{
	unsigned current_level = actor_level_map_lft[inst];

	for (auto it = sorted_actors.begin(); it != sorted_actors.end(); ++it) {
		if (((actor_level_map_lft[*it] + actor_weight_map[*it]) <= current_level)
			&& ((*it)->get_mapping() > inst->get_mapping()))
		{
			return *it;
		}
	}
	return nullptr;
}

static void connected_lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	unsigned max_level,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	unsigned weight_sum)
{
	Config* c = c->getInstance();
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	std::vector<core_balance> cb;

	unsigned core_weight = weight_sum / c->get_cores();

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		core_balance b;
		b.core = core;
		IR::Actor_Instance* current = get_unmapped_actor_highest_level(sorted_actors);
		IR::Actor_Instance* next;
		current->set_mapping(core);
		b.weight = actor_weight_map[current];
		b.end = actor_level_map[current] + actor_weight_map[current];

		while (b.weight < core_weight) {
			next = find_closest_least_predecessors(current, actor_level_map);
			if (next == nullptr) {
				/* No successor left to be mapped, find another actor that coveres the following time slots. */
				next = find_next(current, actor_level_map, actor_weight_map, sorted_actors);
				if (next == nullptr) {
					/* No actor found, hence, we must pay with overlap, but we have to fill our core... */
					next = get_unmapped_actor_highest_level(sorted_actors);
				}
			}
			if (current == nullptr) {
				//no actor to map left, terminate
				return;
			}
			current = next;

			//allow some overshoot of 10% but not more
			if ((b.weight + actor_weight_map[current]) > (core_weight + core_weight / 10)) {
				break;
			}
			current->set_mapping(core);
			b.weight += actor_weight_map[current];
			current->set_sched_order(actor_level_map[current]);
		}

		cb.push_back(b);
	}

	//Check if all actor instances are mapped!
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_mapping() >= c->get_cores() && !(*it)->is_deleted()) {
#ifdef DEBUG_MAPPING
			//To avoid this the overhead factor could be increased
			std::cout << "Actor instance " << (*it)->get_name() << " wasn't mapped!" << std::endl;
#endif
			//Actor instance not mapped ... assign to core with least load
			core_balance* b = get_core_least_weight(cb);
			(*it)->set_mapping(b->core);
			(*it)->set_sched_order(actor_level_map[*it]);
			b->weight += actor_weight_map[*it];
		}
	}
}

static void connected_balanced_lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	unsigned max_level,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	unsigned weight_sum)
{
	Config* c = c->getInstance();
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	std::vector<core_balance> cb;

	unsigned core_weight = weight_sum / c->get_cores();

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		core_balance b;
		b.core = core;
		IR::Actor_Instance* current = get_unmapped_actor_highest_level(sorted_actors);
		IR::Actor_Instance* next;
		current->set_mapping(core);
		b.weight = actor_weight_map[current];
		b.end = actor_level_map[current] + actor_weight_map[current];

		while (b.weight < core_weight) {
			next = find_closest(current, actor_level_map);
			if (next == nullptr) {
				/* No successor left to be mapped, find another actor that coveres the following time slots. */
				next = find_next(current, actor_level_map, actor_weight_map, sorted_actors);
				if (next == nullptr) {
					/* No actor found, hence, we must pay with overlap, but we have to fill our core... */
					next = get_unmapped_actor_highest_level(sorted_actors);
				}
			}
			if (next == nullptr) {
				//no actor to map left, terminate
				return;
			}
			current = next;
			//allow some overshoot of 10% but not more
			if ((b.weight + actor_weight_map[current]) > (core_weight + core_weight / 10)) {
				break;
			}
			current->set_mapping(core);
			b.weight += actor_weight_map[current];
			current->set_sched_order(actor_level_map[current]);
		}

		cb.push_back(b);
	}

	//Check if all actor instances are mapped!
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_mapping() >= c->get_cores() && !(*it)->is_deleted()) {
#ifdef DEBUG_MAPPING
			//To avoid this the overhead factor could be increased
			std::cout << "Actor instance " << (*it)->get_name() << " wasn't mapped!" << std::endl;
#endif
			//Actor instance not mapped ... assign to core with least load
			core_balance* b = get_core_least_weight(cb);
			(*it)->set_mapping(b->core);
			(*it)->set_sched_order(actor_level_map[*it]);
			b->weight += actor_weight_map[*it];
		}
	}
}

static void connected_rr_balanced_lft(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data,
	std::map<IR::Actor_Instance*, unsigned>& actor_level_map,
	unsigned max_level,
	std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
	unsigned weight_sum)
{
	Config* c = c->getInstance();
	std::vector<IR::Actor_Instance*> sorted_actors(dpn->get_actor_instances());
	std::sort(sorted_actors.begin(), sorted_actors.end(), Mapping::Level_Sort_LFT{ actor_level_map });
	std::vector<core_balance> cb; /* Cores are inserted according to their index, e.g. core 0 at index 0. */

	unsigned core_weight = weight_sum / c->get_cores();
	unsigned current_core = 0;

	for (unsigned core = 0; core < c->get_cores(); ++core) {
		core_balance b;
		b.core = core;
		b.weight = 0;

		cb.push_back(b);
	}
	unsigned last_assignment_core = current_core;
	while (1) {
		IR::Actor_Instance* current = get_unmapped_actor_highest_level(sorted_actors);
		if (current == nullptr) {
			/* nothing left, bail out */
			return;
		}
		core_balance& b = cb.at(current_core);

		/* Allow 10% overshot */
		if ((actor_weight_map[current] + b.weight) > (core_weight + core_weight / 10)) {
			if (current_core == last_assignment_core) {
				/* Iterated over all cores and found nothing, hence, there is some actor that has a too large weight,
				 * assign it somewhere. I think this rarely happens, but if it does it would be an endless loop here.
				 * Assuming get_unmapped_actor_highest_level always returns the same actor if not mapped here.
				 */
#ifdef DEBUG_MAPPING
				std::cout << "Assigning actor instance " << current->get_name() << " to least utilized core." << std::endl;
#endif
				core_balance* b = get_core_least_weight(cb);
				current->set_mapping(b->core);
				current->set_sched_order(actor_level_map[current]);
				b->weight += actor_weight_map[current];
			}
			/* actor cannot be mapped to this core, too large weight, try next core. */
			current_core = (current_core + 1) % c->get_cores();
			continue;
		}

		current->set_mapping(current_core);
		current->set_sched_order(actor_level_map[current]);
		b.weight = actor_weight_map[current];

		while (b.weight < core_weight) {
			current = find_closest_with_limit(current, actor_level_map, (core_weight + core_weight / 10) - b.weight, actor_weight_map);
			if (current == nullptr) {
				/* reached the end of this path, advance to next core. */
				break;
			}
			current->set_mapping(b.core);
			b.weight += actor_weight_map[current];
			current->set_sched_order(actor_level_map[current]);
		}
		last_assignment_core = current_core;
		current_core = (current_core + 1) % c->get_cores();
	}

	//Check if all actor instances are mapped!
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->get_mapping() >= c->get_cores() && !(*it)->is_deleted()) {
#ifdef DEBUG_MAPPING
			//To avoid this the overhead factor could be increased
			std::cout << "Actor instance " << (*it)->get_name() << " wasn't mapped!" << std::endl;
#endif
			//Actor instance not mapped ... assign to core with least load
			core_balance* b = get_core_least_weight(cb);
			(*it)->set_mapping(b->core);
			(*it)->set_sched_order(actor_level_map[*it]);
			b->weight += actor_weight_map[*it];
		}
	}
}

void Mapping::generate_level_based_mapping(
	IR::Dataflow_Network* dpn,
	Mapping::Mapping_Data* data)
{
	Config* c = c->getInstance();

	std::map<IR::Actor_Instance*, unsigned> actor_weight_map;
	unsigned weight_sum = 0;
	std::set<IR::Actor_Instance*> output_nodes;
	unsigned max_level_lft = 0;
	std::map<IR::Actor_Instance*, unsigned> actor_level_map_lft;

	if (c->get_use_weights_from_file()) {
		weight_sum = Mapping::read_actor_weights(dpn, c->get_node_weights_file(), actor_weight_map);
	}
	else {
		weight_sum = Mapping::compute_actor_weights(dpn, c->get_mapping_weights(), actor_weight_map);
	}

	if (c->get_use_outputs_from_file()) {
		Mapping::read_output_nodes(dpn, c->get_output_nodes_file(), output_nodes);
	}
	else {
		Mapping::detect_output_nodes(dpn, output_nodes);
	}

	max_level_lft = Mapping::compute_levels_alap(dpn, c->get_mapping_weights(), actor_weight_map, output_nodes, actor_level_map_lft);

	if (c->get_mapping_connected()) {
		if (c->get_rr_mapping() && c->get_balanced_mapping()) {
			connected_rr_balanced_lft(dpn, data, actor_level_map_lft, max_level_lft, actor_weight_map, weight_sum);
		}
		else if (c->get_balanced_mapping()) {
			connected_balanced_lft(dpn, data, actor_level_map_lft, max_level_lft, actor_weight_map, weight_sum);
		}
		else {
			connected_lft(dpn, data, actor_level_map_lft, max_level_lft, actor_weight_map, weight_sum);
		}
	}
	else if (c->get_mapping_level()) {
		lft_horizontal_partitioning(dpn, data, actor_level_map_lft, max_level_lft);
	}
	else if (c->get_balanced_mapping()) {
		if (c->get_rr_mapping()) {
			simple_balanced_lft(dpn, data, actor_level_map_lft, actor_weight_map);
		}
		else {
			balanced_lft(dpn, data, actor_level_map_lft, actor_weight_map);
		}
	}
	else {
		lft(dpn, data, actor_level_map_lft);
	}


	if (c->get_verbose_map()) {
		if (c->get_mapping_weights()) {
			std::cout << "Sum of weights: " << weight_sum << std::endl;
			std::cout << "Actor instance weights:" << std::endl;
			for (auto it = actor_weight_map.begin(); it != actor_weight_map.end(); ++it) {
				std::cout << it->first->get_name() << " " << it->second << std::endl;
			}
		}
		std::cout << "Maximum level (ALAP/LFT): " << max_level_lft << std::endl;
		std::cout << "Actor instance levels (ALAP/LFT):" << std::endl;
		for (auto it = actor_level_map_lft.begin(); it != actor_level_map_lft.end(); ++it) {
			std::cout << it->first->get_name() << " " << it->second << std::endl;
		}
	}
}