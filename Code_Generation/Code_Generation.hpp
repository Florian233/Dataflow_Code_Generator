#pragma once

#include "IR/Dataflow_Network.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Mapping/Mapping.hpp"
#include "Exceptions.hpp"
#include <set>

namespace Code_Generation {

	/* Entry point of the code generation. */
	void generate_code(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	/* Generate a orcc compaitibility layer that can pare the command line input and provides the options_t struct.*/
	void generate_ORCC_compatibility_layer(std::string path);

	/* Generate the main file. */
	void generate_core(
		IR::Dataflow_Network* dpn,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data,
		std::string include_code);

	/* Generate the channel implementations. */
	void generate_channel_code(
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	/* Generate the code for a specific actor instance. */
	std::string generate_actor_code(
		IR::Actor_Instance *instance,
		std::string class_name,
		std::set<std::string>& unused_actions,
		std::set<std::string>& unused_in_channels,
		std::set<std::string>& unused_out_channels,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	/* Generate the code for a composit actor - future work. */
	std::string generate_composit_actor_code(
		IR::Composit_Actor *actor,
		Optimization::Optimization_Data_Phase1* opt_data1,
		Optimization::Optimization_Data_Phase2* opt_data2,
		Mapping::Mapping_Data* map_data);

	/* Generate a simple cmake file for the generated code. It doesn't contain native files obviously. */
	void generate_cmake_file(
		std::string network_name,
		std::string source_files,
		std::string path);

	class Code_Generation_Exception : public Converter_Exception {
	public:
		Code_Generation_Exception(std::string _str) : Converter_Exception{ _str } {};
	};
}