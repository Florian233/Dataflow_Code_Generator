#include "Generate_C_Cpp.hpp"

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::generate_composit_actor_code(
	IR::Composit_Actor* actor,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::string channel_include)
{

	/* Intentionally left empty, for future use. */

	return std::make_pair(std::string(), std::string());
}