#pragma once

#include "IR/Dataflow_Network.hpp"

namespace Mapping {

	struct Mapping_Data {
		// indicates that actors are executed by several PEs, this has implications on main generation.
		// This flag means that all actor instances shall be executed by all PEs
		bool actor_sharing;
	};

	/* Perform the mapping for the given network. */
	Mapping_Data* mapping(IR::Dataflow_Network* dpn);

	/* Function to read the mapping from a file. 
	 * The function might change the configuration (number of cores) if the mapping file specifies too much
	 * or less clusters.
	 */
	void read_mapping(IR::Dataflow_Network* dpn, Mapping_Data* data, std::string path);

	void generate_level_based_mapping(IR::Dataflow_Network* dpn, Mapping_Data* data);

};