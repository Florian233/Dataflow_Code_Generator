#pragma once
#include "Code_Generation/Code_Generation.hpp"


namespace Code_Generation_C_Cpp {
	using Header = std::string;
	using Source = std::string;
	/* Functions that are called when code generation is started and ended,
	 * all other functions are called in between.
	 */
	std::pair<Header, Source> start_code_generation(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	std::pair<Header, Source> end_code_generation(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::vector<Code_Generation_C_Cpp::Header>& headers,
		std::vector<Code_Generation_C_Cpp::Source>& sources);

	/* Generate the main file. */
	std::pair<Header, Source> generate_core(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::vector<std::string>& includes);

	/* Generate the channel implementations. */
	std::pair<Header, Source> generate_channel_code(
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	/* Generate the code for a specific actor instance. */
	std::pair<Header, Source> generate_actor_code(
		IR::Actor_Instance* instance,
		std::string class_name,
		std::set<std::string>& unused_actions,
		std::set<std::string>& unused_in_channels,
		std::set<std::string>& unused_out_channels,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::string channel_include,
		unsigned scheduling_loop_bound);

	/* Generate the code for a composit actor - future work. */
	std::pair<Header, Source> generate_composit_actor_code(
		IR::Composit_Actor* actor,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::string channel_include,
		unsigned scheduling_loop_bound);
};