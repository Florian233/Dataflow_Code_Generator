#include "Code_Generation/Code_Generation.hpp"
#include <fstream>

/* Generate a simple cmake file to build the generated code. */

std::string code1 =
"cmake_minimum_required(VERSION 3.8)\n\n"
"# Enable Hot Reload for MSVC compilers if supported.\n"
"if (POLICY CMP0141)\n"
"  cmake_policy(SET CMP0141 NEW)\n"
"  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT \"$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>\")\n"
"endif()\n\n";

std::string code2 =
"set(CMAKE_CXX_STANDARD 11)\n"
"set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
"include_directories(${CMAKE_CURRENT_LIST_DIR})";

void Code_Generation::generate_cmake_file(
	std::string network_name,
	std::string source_files,
	std::string path)
{
	std::ofstream output_file{ path + "\\CMakeLists.txt" };
	if (output_file.bad()) {
		throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path + "\\CMakeLists.txt" };
	}
	output_file << code1;
	output_file << ("project(\"" + network_name + "\")\n");
	output_file << ("add_executable(" + network_name + " " + source_files + ")\n");
	output_file << code2;
	output_file.close();
}