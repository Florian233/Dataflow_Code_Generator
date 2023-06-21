#pragma once

#include "IR/Dataflow_Network.hpp"

namespace Optimization {

	// not used yet, just here for future use
	struct Optimization_Data_Phase1 {
		/* Intentionally left empty, for future use. */
	};

	/* Perform optimizations before the mapping is done! */
	Optimization_Data_Phase1* optimize_phase1(IR::Dataflow_Network* dpn);

};