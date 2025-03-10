#include "Scheduling.hpp"
#include "Config/config.h"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Lib.hpp"
#include "ABI/abi.hpp"

static std::string find_class_name(
	std::string actor_name,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	return actor_data_map[actor_name]->get_class_name();
}

static std::string basic_non_preemptive(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& global_scheduling_routines,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	std::string result;
	Config* c = c->getInstance();

	if (c->get_list_scheduling() && !map_data->actor_sharing) {
		for (unsigned i = 0; i < c->get_cores(); ++i) {
			result.append("std::vector<Actor *> core" + std::to_string(i) + "_actors;\n");
		}
	}

	/* Differentation between the case that actors are shared between cores and the case where each
	 * actor instance is assigned to exactly one core.
	 * The first case requires atomic flags to avoid parallel execution of the same actor instance.
	 */
	if (map_data->actor_sharing) {
		// Actor sharing is active, hence, we must make sure to run an actor only on one PE at the same time
		for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it) {
			std::string tmp;
			ABI_ATOMIC_DECL(c, tmp, it->first, "");
			result.append(tmp);
		}
		result.append("\n");


		std::vector<std::string> actors;
		if (c->get_topology_sort()) {
			std::set<std::string> tmp;
			for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it) {
				tmp.insert(it->first);
			}
			Scheduling::topology_sort(tmp, dpn, actors);
		}
		else {
			for (auto it = actor_data_map.begin(); it != actor_data_map.end(); ++it) {
				actors.push_back(it->first);
			}
		}

		// we only have to generate one global scheduling routine as it is the same for all cores
		result.append("void global_scheduler(void) {\n");
		result.append("\twhile (1) {\n");
		for (auto it = actors.begin(); it != actors.end(); ++it) {
			std::string tmp;
			ABI_ATOMIC_TEST_SET(c, tmp, *it, "");
			result.append("\t\tif(!" + tmp + ") { \n");
			if (c->get_target_language() == Target_Language::cpp) {
				if (c->get_static_alloc()) {
					result.append("\t\t\t" + *it + ".schedule();\n");
				}
				else {
					result.append("\t\t\t" + *it + "->schedule();\n");
				}
			}
			else {
				if (c->get_static_alloc()) {
					result.append("\t\t\t" + find_class_name(*it, actor_data_map) + "_schedule(&" + *it + ");\n");
				}
				else {
					result.append("\t\t\t" + find_class_name(*it, actor_data_map) + "_schedule(" + *it + ");\n");
				}
			}
			ABI_ATOMIC_CLEAR(c, tmp, *it, "\t\t\t");
			result.append(tmp);
			result.append("\t\t}\n");
		}
		result.append("\t}\n");
		result.append("}\n");

		for (unsigned i = 0; i < c->get_cores(); ++i) {
			global_scheduling_routines.push_back("global_scheduler");
		}
	}
	else {
		for (unsigned core = 0; core < c->get_cores(); ++core) {
			result.append("void schedule_core" + std::to_string(core) + "(void) {\n");

			std::set<std::string> actors;
			Scheduling::find_actors_for_core(core, dpn, actors);

			std::vector<std::string> maybe_sorted;
			if (c->get_topology_sort()) {
				Scheduling::topology_sort(actors, dpn, maybe_sorted);
			}
			else {
				if (!Scheduling::sched_order_sort(actors, dpn, maybe_sorted)) {
					for (auto it = actors.begin(); it != actors.end(); ++it) {
						maybe_sorted.push_back(*it);
					}
				}
			}
			
			if (c->get_list_scheduling()) {
				for (auto it = maybe_sorted.begin(); it != maybe_sorted.end(); ++it) {
					if (c->get_static_alloc()) {
						result.append("\tcore" + std::to_string(core) + "_actors.push_back(&" + *it + ");\n");
					}
					else {
						result.append("\tcore" + std::to_string(core) + "_actors.push_back(" + *it + ");\n");
					}
				}
			}

			result.append("\twhile (1) {\n");
			if (c->get_list_scheduling()) {
				result.append("\t\tfor (auto actor_it = core" + std::to_string(core) + "_actors.begin(); actor_it != core" + std::to_string(core) + "_actors.end(); ++actor_it) {\n");
				result.append("\t\t\t(*actor_it)->schedule();\n");
				result.append("\t\t}\n");
			}
			else {
				for (auto it = maybe_sorted.begin(); it != maybe_sorted.end(); ++it) {
					if (c->get_target_language() == Target_Language::cpp) {
						if (c->get_static_alloc()) {
							result.append("\t\t" + *it + ".schedule();\n");
						}
						else {
							result.append("\t\t" + *it + "->schedule();\n");
						}
					}
					else {
						if (c->get_static_alloc()) {
							result.append("\t\t" + find_class_name(*it, actor_data_map) + "_schedule(&" + *it + ");\n");
						}
						else {
							result.append("\t\t" + find_class_name(*it, actor_data_map) + "_schedule(" + *it + ");\n");
						}
					}
				}
			}

			result.append("\t}\n");
			result.append("}\n");

			global_scheduling_routines.push_back("schedule_core" + std::to_string(core));
		}
	}

	return result;
}


std::string Scheduling::generate_global_scheduler(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& global_scheduling_routines,
	std::map<std::string, Actor_Conversion_Data*>& actor_data_map)
{
	return basic_non_preemptive(dpn, opt_data1, opt_data2, map_data, global_scheduling_routines, actor_data_map);
}