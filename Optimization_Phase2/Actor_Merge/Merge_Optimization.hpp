#pragma once

#include "IR/Dataflow_Network.hpp"

namespace Merge_Optimization {

	void core_merge(IR::Dataflow_Network* dpn);

	void config_merge(IR::Dataflow_Network* dpn);

}
