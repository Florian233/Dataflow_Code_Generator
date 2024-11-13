#include "Code_Generation/Code_Generation.hpp"
#include "Misc.hpp"
#include <fstream>
#include "Config/config.h"
#include <filesystem>

/* Generate a simple cmake file to build the generated code. */

std::string code1 =
"cmake_minimum_required(VERSION 3.8)\n\n"
"# Enable Hot Reload for MSVC compilers if supported.\n"
"if (POLICY CMP0141)\n"
"  cmake_policy(SET CMP0141 NEW)\n"
"  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT \"$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>\")\n"
"endif()\n\n";

std::string code2_cpp =
"set(CMAKE_CXX_STANDARD 11)\n"
"set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
"include_directories(${CMAKE_CURRENT_LIST_DIR})";

std::string code2_c =
"set(CMAKE_C_STANDARD C99)\n"
"set(C_STANDARD_REQUIRED ON)\n\n"
"include_directories(${CMAKE_CURRENT_LIST_DIR})";

void generate_cmake_file(
	std::string network_name,
	std::string source_files,
	std::string path)
{
	Config* c = c->getInstance();
	std::filesystem::path cmake_path{ path };
	cmake_path /= "CMakeLists.txt";

	std::ofstream output_file{ cmake_path };
	if (output_file.fail()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + cmake_path.string()};
	}
	output_file << code1;
	output_file << ("project(\"" + network_name + "\")\n");
	output_file << ("add_executable(" + network_name + " " + source_files + ")\n");
	if (c->get_target_language() == Target_Language::cpp) {
		output_file << code2_cpp;
	}
	else if (c->get_target_language() == Target_Language::c) {
		output_file << code2_c;
	}
	output_file.close();
}