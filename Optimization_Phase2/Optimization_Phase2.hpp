#pragma once

#include "IR/Dataflow_Network.hpp"

namespace Optimization {
// Prefix for the name of composit actors to avoid name conflicts
#define COMPOSIT_ACTOR_PREFIX "composit_"

	// Not used yet, reserved for future use
	struct Optimization_Data_Phase2 {

		/* Flag whether a special control channel implementation shall be emitted by the channel class creator. */
		bool control_channel{ false };

	};

	/* Perform optimizations after the mapping is done! */
	Optimization_Data_Phase2* optimize_phase2(IR::Dataflow_Network* dpn);

};