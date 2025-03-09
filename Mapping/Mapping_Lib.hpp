#pragma once

#include <map>
#include "IR/Dataflow_Network.hpp"

namespace Mapping {

	/* Compute the weights for each actor and write it to the map.
	 * If use_weights is not set, all actor instances get the weight one, this allows the usage
	 * of this function on all paths.
	 * The weights are computed by counting the channels connected to the corresponding actor.
	 * The sum of all weights is returned.
	 * This function provides only a basic abstraction, for more sophisticated methods use reading from
	 * XML as provided by the function below.
	 */
	unsigned compute_actor_weights(
		IR::Dataflow_Network* dpn,
		bool use_weights,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map);

	/* Read the weights for each actor instance for a XML file and store them in the
	 * map.
	 * The sum of all weights is returned.
	 */
	unsigned read_actor_weights(
		IR::Dataflow_Network* dpn,
		std::string path,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map);

	/* The function assignes to each instance the level that is used for mapping.
	 * The level is assigned by walking backwards from the output nodes and assigning
	 * to each node the longest distance to an output node.
	 * Therefore it technically generates an ALAP assignmen, computing the latest finish time.
	 * The function returns the overall number of levels.
	 */
	unsigned compute_levels_alap(
		IR::Dataflow_Network* dpn,
		bool use_weights,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
		std::set<IR::Actor_Instance*>& output_nodes,
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map);

	/* The function assignes to each instance the level that is used for mapping.
	 * The level is assigned by walking from the input nodes and assigning
	 * to each node the longest distance to an input node.
	 * Therefore it technically generates an ASAP assignment, computing the earliest start time.
	 * The function returns the overall number of levels.
	 */
	unsigned compute_levels_asap(
		IR::Dataflow_Network* dpn,
		bool use_weights,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
		std::set<IR::Actor_Instance*>& input_nodes,
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map);

	/* Find the critical path through the network and mark the corresponding actors. */
	unsigned compute_critical_path(
		IR::Dataflow_Network* dpn,
		bool use_weights,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
		std::set<IR::Actor_Instance*>& output_nodes,
		std::vector<IR::Actor_Instance*>& critical_path);

	/* Compute the slack for scheduling based on the ALAP and ASAP values and the weight of the actor instance. */
	void compute_slack(
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map_lft,
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map_est,
		unsigned max_level_est,
		std::map<IR::Actor_Instance*, unsigned>& actor_weight_map,
		std::map<IR::Actor_Instance*, unsigned>& slack_map);

	/* Comparison object for actor instances according to ALAP values determined by the above function. */
	class Level_Sort_LFT {
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map;
	public:
		Level_Sort_LFT(std::map<IR::Actor_Instance*, unsigned>& a) : actor_level_map{ a } {};

		bool operator()(IR::Actor_Instance* a, IR::Actor_Instance* b) const {
			// Actor instances with large levels first in the list
			return actor_level_map[a] > actor_level_map[b];
		}
	};

	/* Comparison object for actor instances according to ASAP values determined by the above function. */
	class Level_Sort_EST {
		std::map<IR::Actor_Instance*, unsigned>& actor_level_map;
	public:
		Level_Sort_EST(std::map<IR::Actor_Instance*, unsigned>& a) : actor_level_map{ a } {};

		bool operator()(IR::Actor_Instance* a, IR::Actor_Instance* b) const {
			// Actor instances with large levels first in the list
			return actor_level_map[a] < actor_level_map[b];
		}
	};
}