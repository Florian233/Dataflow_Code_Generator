#include "Mapping.hpp"
#include "Config/config.h"
#include "Config/debug.h"

/* Map each actor instance to each core. This mapping strategy requires bookkeeping of which actor is executed
 * on which core, e.g. by atomics. Hence, the flag actor sharing flag is set in the Mapping_Data.
 */
void map_all_to_all(IR::Dataflow_Network* dpn, Mapping::Mapping_Data *d, unsigned cores) {
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if (!(*it)->is_deleted()) {
			// Use zero for this case, it doesn't matter the the actor sharing flag is set in a second anyhow
			(*it)->set_mapping(0);
		}
	}
	if (cores > 1) {
		// Scheduler doesn't have to ensure exclusive execution of the actors if this is given
		// as only one core is used!
		d->actor_sharing = true;
	}
}

Mapping::Mapping_Data* Mapping::mapping(IR::Dataflow_Network* dpn) {
	Config* c = c->getInstance();
	Mapping::Mapping_Data *data = new Mapping::Mapping_Data{};

	/* Prioritize reading the mapping from a file, otherwise check strategies one by one. */
	if (c->is_map_file()) {
		read_mapping(dpn, data, c->get_mapping_file());
	}
	else if (c->get_mapping_strategy_all_to_all()) {
		map_all_to_all(dpn, data, c->get_cores());
	}
	else {
		// This cannot happen, just to detect when something in this codes goes totally wrong.
		std::cout << "Error: Cannot detect mapping strategy." << std::endl;
		exit(1);
	}

#ifdef DEBUG_MAPPING
	std::cout << "Mapping: " << std::endl;
	for (auto it = dpn->get_actor_instances().begin();
		it != dpn->get_actor_instances().end(); ++it)
	{
		if ((*it)->is_deleted()) {
			continue;
		}
		std::cout << "Actor " << (*it)->get_name() << " mapped to ";
		if (data->actor_sharing) {
			std::cout << "all cores.";
		}
		else {
			std::cout << "core " << (*it)->get_mapping();
		}
		std::cout << std::endl;
	}
#endif

	return data;
}