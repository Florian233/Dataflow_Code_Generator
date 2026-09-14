#pragma once

#include <vector>
#include <string>
#include <set>
#include "IR/Dataflow_Network.hpp"
#include "IR/Actor.hpp"
#include "Dataflow_Analysis/Actor_Classification/Actor_Classification.hpp"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Data.hpp"
#include "Mapping/Mapping.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Exceptions.hpp"

namespace Scheduling {

	/* Generate a local scheduler for the given actor for the scheduling strategy defined in the
	 * Configuration.
	 */
	std::string generate_local_scheduler(
		//map function_name -> guard
		std::map<std::string, AST::Action*>& action_guard,
		std::vector<IR::FSM_Entry>& fsm,
		std::vector<IR::Priority_Entry>& priorities,
		Actor_Classification input_classification,
		Actor_Classification output_classification,
		std::string prefix,
		std::string schedule_function_name,
		std::string schedule_function_parameter,
		std::map<std::string, std::vector< Channel_Schedule_Data>>& actions,
		std::map<std::string, std::string> replacements,
		unsigned scheduling_loop_bound,
		bool no_else = false,
		bool is_source = false,
		bool is_sink = false);

	/* Generate a global scheduler based on the mapping stored in the IR of the actor instances
	 * and the Configuration.
	 */
	std::string generate_global_scheduler(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::vector<std::string>& global_scheduling_routines,
		std::map<std::string, std::string> schedulable_instances);


	class Scheduler_Generation_Exception : public Converter_Exception {
	public:
		Scheduler_Generation_Exception(std::string _str) : Converter_Exception{ _str } {};
	};
}