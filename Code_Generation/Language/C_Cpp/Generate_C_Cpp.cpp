#include "Generate_C_Cpp.hpp"
#include "Config/config.h"
#include <filesystem>
#include "Misc.hpp"
#include "ABI/abi.hpp"

/* Generate a simple base class all other actors inherit from.
 * This allows storing all actors in one list.
 */
static void generate_base_class(void) {
	std::string code =
		"#pragma once\n\n"
		"class Actor {\n"
		"public:\n"
		"\tvirtual void initialize(void) = 0;\n"
		"\tvirtual void schedule(void) = 0;\n"
		"};";

	Config* c = c->getInstance();
	std::filesystem::path path{ c->get_target_dir() };
	path /= "Actor.hpp";

	std::ofstream output_file{ path };
	if (output_file.bad()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
	}
	output_file << code;
	output_file.close();
}

static void init_abi(IR::Dataflow_Network* dpn)
{
	Config* c = c->getInstance();
	ABI_INIT(c, dpn);
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::start_code_generation(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	init_abi(dpn);

	Config* c = c->getInstance();
	if (c->get_target_language() == Target_Language::cpp) {
		//For C we cannot use inheritance
		generate_base_class();
	}

	if (c->get_orcc_compat()) {
		return generate_ORCC_compatibility_layer(c->get_target_dir());
	}
	return std::make_pair(std::string(), std::string());
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::end_code_generation(
	IR::Dataflow_Network* dpn,
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data,
	std::vector< Code_Generation_C_Cpp::Header>& headers,
	std::vector< Code_Generation_C_Cpp::Source>& sources)
{
	Config* c = c->getInstance();
	if (c->get_cmake()) {
		std::string path = c->get_target_dir();
		std::string source_files;
		for (auto s : sources) {
			if (!source_files.empty()) {
				source_files.append(" ");
			}
			source_files.append(s);
		}
		std::string network_name = dpn->get_name();

		generate_cmake_file(network_name, source_files, path);
	}

	return std::make_pair(std::string(), std::string());
}

std::pair<Code_Generation_C_Cpp::Header, Code_Generation_C_Cpp::Source>
Code_Generation_C_Cpp::generate_channel_code(
	Optimization::Optimization_Data_Phase1* opt_data1,
	Optimization::Optimization_Data_Phase2* opt_data2,
	Mapping::Mapping_Data* map_data)
{
	Config* c = c->getInstance();

	ABI_CHANNEL_GEN(c, map_data->actor_sharing)
}