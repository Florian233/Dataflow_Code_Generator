#pragma once

#include <string>

void generate_cmake_file(
	std::string network_name,
	std::string source_files,
	std::string path);


std::pair<std::string, std::string> generate_ORCC_compatibility_layer(std::string path);