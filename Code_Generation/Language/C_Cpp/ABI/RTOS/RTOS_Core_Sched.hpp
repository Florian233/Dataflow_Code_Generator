#pragma once

#include <string>
#include "IR/Dataflow_Network.hpp"
#include "Dataflow_Analysis/Scheduling_Lib/Scheduling_Data.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Mapping/Mapping.hpp"

void rtos_register_actor_globals(std::string s);

std::string generate_rtos_main(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector<std::string>& includes,
	std::map<std::string, std::string> schedulable_instances,
	std::map<std::string, std::map<std::string, std::string>> default_param_maps,
	std::map<std::string, std::vector<std::string>> param_order_map);

std::string generate_rtos_scheduler(
	std::map<std::string, std::vector<Scheduling::Channel_Schedule_Data> >& actions,
	std::vector<IR::FSM_Entry>& fsm,
	std::vector<IR::Priority_Entry>& priorities,
	//maps method names to their scheduling condition: guards and size and free space in the FIFOs
	std::map<std::string, std::string>& action_guard,
	std::map<std::string, std::string>& action_schedulingCondition_map,
	std::map<std::string, std::string>& action_freeSpaceCondition_map,
	std::map<std::string, std::string>& state_channel_access,
	std::map<std::string, std::string>& action_post_exec_code_map,
	std::string schedule_function_name,
	unsigned loopcount,
	std::string sched_loop,
	bool is_source,
	bool is_sink);


void rtos_generate_makefile(
	IR::Dataflow_Network* dpn);