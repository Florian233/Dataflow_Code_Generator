#include <iostream>
#include <memory>
#include <sstream>
#include <filesystem>
#include <chrono>

#include "Config/config.h"
#include "Config/debug.h"
#include "IR/Dataflow_Network.hpp"
#include "Reader/Reader.hpp"
#include "Dataflow_Analysis/Actor_Classification/Actor_Classification.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Mapping/Mapping.hpp"
#include "Code_Generation/Code_Generation.hpp"
#include "Dataflow_Analysis/Dataflow_Analysis.hpp"
using namespace std;

Config* Config::instance = 0;
static bool silent = false;
static char* get_next_value(int argc, char* argv[], int& i) {
	if (i + 1 >= argc) {
		std::cout << "Error: Option " << argv[i] << " requires a value but none was given." << std::endl;
		exit(1);
	}
	return argv[++i];
}

static void parse_command_line_input(int argc, char* argv[]) {
	Config* c = Config::getInstance();
	bool mapping_set{ false };
	bool schedule_set{ false };
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			std::cout << "\nUsage: %s [options]\n"
				"\nCommon arguments:\n"
				"	-h                 Print this message.\n"
				"	-w <directory>     Specify the directory to write the output to.\n"
				"	-d <directory>     Specify the directory of the RVC-CAL sources.\n"
				"	-n <file>          Specify the top network that shall be converted.\n"
				"	--orcc			   ORCC compatibility, required to use ORCC projects.\n"
				"   --cmake            Generate CMake File for the generated code.\n"
				"   --static_alloc     Make allocations static, don't use new operator.\n"
				"   --abi=<stdc|stdc++|rtos>      Generate C or C++ code, default is C++; stdc is only partially supported, might required manual adjustments.\n"
				"   --prefix=<str>     Global symbol prefix.\n"
				"   --subdir=<path>    RTOS makefile for subdirectory inclusion.\n"
				"\nCommunication Channels:\n"
				"	-s <number>        Specify the default size of the FIFOs.\n"
				"   --size_file <file> Specify the size of the FIFOs for each channel in a file; if not defined -s is used as default.\n"
				"\nOpenMP:\n"
				"	--omp_tasking      Use OpenMP tasking for the parallel execution of the global schedulers.\n"
				"\nMapping:\n"
				"	-c <number>        Specify the number of cores to use.\n"
				"	--map=(all|random)\n"
				"                 all: Map all actor instances to all cores (default).\n"
				"                 random: Map actor instances to cores randomly.\n"
				"   --map_file <file>  Uses the mapping from <file>, ignores -c and --map.\n"
				"   --output_nodes_file <file> Provide output nodes as a file, avoids autodetection of output nodes as this might fail due to feedback loops.\n"
				"   --input_nodes_file <file> Provide input nodes as a file, avoids autodetection of input nodes as this might fail due to feedback loops.\n"
				"   --feedback_edges <file> Provide feedback edges as a file, avoids autodetection of feedback edges which might fail for certain topologies.\n"
				"\nScheduling:\n"
				"   --topology_sort    Use topology sorted list for scheduling.\n"
				"   --schedule=(non_preemptive|round_robin)\n"
				"                 non_preemtive: Use non-preemptive scheduling strategy (default).\n"
				"                 round_robin: Use round-robin scheduling strategy.\n"
				"   --list_scheduling  Use a list for scheduling instead of hard-coded scheduler.\n"
				"   --bound_sched <number>      Use bound loops for local scheduling with given number of tries.\n"
				"   --bound_sched_file <file>   Use bound loops for local scheduling for each actor instance.\n"
				"\nRTOS Mapping:\n"
				"   --mutex    Use mutex to protect schedulability check.\n"
				"   --atomics  Use atomics to protect schedulability check.\n"
				"   --rtos=<basic|counting|schedcheck> RTOS mapping strategy.\n"
				"   --release=<timespan> Timespan in ns between two releases of source actors.\n"
				"   --deadline=<timespan> Timespan the task can take to finish after release.\n"
				"   --cycles=<number> Number of cycles for the local scheduling loop, only relevant for RTOS mapping. Default is one.\n"
				"\nOptimizations:\n"
				"   --prune_unconnected Remove unconnected channels from actors, otherwise they are set to NULL.\n"
				"   --opt_sched         Use optimized local scheduling.\n"
				"   --opt_cmerge        Merge adjacent actor instances on the same core.\n"
				"   --opt_config <file> Merge actor instances based on the config file.\n"
				"   --no-pe             Omit prolog and epilog split in actor merge.\n"
				"\nVerbosity:\n"
				"   --verbose=(all, reader, ir, classify, opt1, opt2, map, code-gen).\n"
				"   --silent            No output at all except runtime.\n";
			exit(0);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			c->set_target_dir(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "-s") == 0) {
			c->set_FIFO_size(static_cast<unsigned int>(atoi(get_next_value(argc, argv, i))));
		}
		else if (strcmp(argv[i], "-d") == 0) {
			c->set_source_dir(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "-n") == 0) {
			c->set_network_file(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "-c") == 0) {
			c->set_cores(static_cast<unsigned int>(atoi(get_next_value(argc, argv, i))));
		}
		else if (strcmp(argv[i], "--orcc") == 0) {
			c->set_orcc_compat();
		}
		else if (strcmp(argv[i], "--cmake") == 0) {
			c->set_cmake();
		}
		else if (strcmp(argv[i], "--static_alloc") == 0) {
			c->set_static_alloc();
		}
		else if (strncmp(argv[i], "--prefix=", 9) == 0) {
			std::string prefix{ argv[i] + 9 };
			c->set_globals_prefix(prefix);
		}
		else if (strncmp(argv[i], "--subdir=", 9) == 0) {
			std::string subdir{ argv[i] + 9 };
			c->set_rtos_subdir(subdir);
		}
		else if (strncmp(argv[i], "--abi=", 6) == 0) {
			std::string strat{ argv[i] + 6 };
			if ((strat == "stdc++") || (strat == "stdC++") || (strat == "stdCpp") || (strat == "stdcpp")) {
				c->set_target_language(Target_Language::cpp);
				c->set_target_ABI(Target_ABI::stdcpp);
			}
			else if ((strat == "stdc") || (strat == "stdC")) {
				c->set_target_language(Target_Language::c);
				c->set_target_ABI(Target_ABI::stdc);
			}
			else if ((strat == "rtos") || (strat == "RTOS")) {
				c->set_target_language(Target_Language::c);
				c->set_target_ABI(Target_ABI::rtos);
			}
			else {
				std::cout << "Cannot detect target ABI and language for code generation." << std::endl;
				exit(1);
			}
		}
		else if (strcmp(argv[i], "--mutex") == 0) {
			c->set_use_mutex();
		}
		else if (strcmp(argv[i], "--atomics") == 0) {
			c->set_use_atomics();
		}
		else if (strncmp(argv[i], "--rtos=", 7) == 0) {
			if (strcmp(argv[i] + 7, "basic") == 0) {
				c->set_rtos_strategy(RTOS_Strategy::basic);
			}
			else if (strcmp(argv[i] + 7, "counting") == 0) {
				c->set_rtos_strategy(RTOS_Strategy::counting);
			}
			else if (strcmp(argv[i] + 7, "schedcheck") == 0) {
				c->set_rtos_strategy(RTOS_Strategy::schedcheck);
			}
			else {
				std::cout << "Error: Unknown RTOS mapping strategy " << argv[i] << std::endl;
				exit(1);
			}
		}
		else if (strncmp(argv[i], "--release=", 10) == 0) {
			c->set_release(static_cast<unsigned int>(atoi(argv[i] + 10)));
		}
		else if (strncmp(argv[i], "--deadline=", 11) == 0) {
			c->set_deadline(static_cast<unsigned int>(atoi(argv[i] + 11)));
		}
		else if (strncmp(argv[i], "--cycles=", 9) == 0) {
			c->set_rtos_sched_cycles(static_cast<unsigned int>(atoi(argv[i] + 9)));
		}
		else if (strcmp(argv[i], "--map_file") == 0) {
			c->set_mapping_file(get_next_value(argc, argv, i));
			mapping_set = true;
		}
		else if (strcmp(argv[i], "--size_file") == 0) {
			c->set_channel_size_file(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "--feedback_edges") == 0) {
			c->set_feedback_edges_file(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "--output_nodes_file") == 0) {
			c->set_output_nodes_file(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "--input_nodes_file") == 0) {
			c->set_input_nodes_file(get_next_value(argc, argv, i));
		}
		else if (strncmp(argv[i], "--map=", 6) == 0) {
			std::string strat{argv[i] + 6};
			if (strat == "all") {
				c->set_mapping_strategy_all_to_all();
				mapping_set = true;
			}
			else if (strat == "random") {
				c->set_random_mapping();
				mapping_set = true;
			}
			else {
				std::cout << "Error: Unknown mapping strategy " << argv[i] << std::endl;
				exit(1);
			}
		}
		else if (strncmp(argv[i], "--schedule=", 11) == 0) {
			if (strcmp(argv[i] + 11, "non_preemptive") == 0) {
				c->set_sched_non_preemptive();
				schedule_set = true;
			}
			else if (strcmp(argv[i] + 11, "round_robin") == 0) {
				c->set_sched_rr();
				schedule_set = true;
			}
			else {
				std::cout << "Error: Unknown mapping strategy " << argv[i] << std::endl;
				exit(1);
			}
		}
		else if (strcmp(argv[i], "--list_scheduling") == 0) {
			c->set_list_scheduling();
		}
		else if (strcmp(argv[i], "--topology_sort") == 0) {
			c->set_topology_sort();
		}
		else if (strcmp(argv[i], "--bound_sched") == 0) {
			c->set_bound_local_sched_loops(static_cast<unsigned int>(atoi(get_next_value(argc, argv, i))));
		}
		else if (strcmp(argv[i], "--bound_sched_file") == 0) {
			c->set_bound_sched_loops_file(get_next_value(argc, argv, i));
		}
		else if (strcmp(argv[i], "--omp_tasking") == 0) {
			c->set_omp_tasking();
		}
		else if (strcmp(argv[i], "--prune_unconnected") == 0) {
			c->set_prune_disconnected();
		}
		else if (strcmp(argv[i], "--opt_sched") == 0) {
			c->set_optimize_scheduling();
		}
		else if (strcmp(argv[i], "--no-pe") == 0) {
			c->clear_prolog_epilog_opt();
		}
		else if (strcmp(argv[i], "--opt_cmerge") == 0) {
			c->set_optimize_core_merge();
		}
		else if (strcmp(argv[i], "--opt_config") == 0) {
			c->set_optimize_config_merge();
			c->set_merge_config_file(get_next_value(argc, argv, i));
		}
		else if (strncmp(argv[i], "--verbose=", 10) == 0) {
			std::string lvl{argv[i] + 10};
			if (lvl == "all") {
				c->set_verbose_classify();
				c->set_verbose_code_gen();
				c->set_verbose_ir_gen();
				c->set_verbose_map();
				c->set_verbose_opt1();
				c->set_verbose_opt2();
			}
			if (lvl.find("opt1") != std::string::npos) {
				c->set_verbose_opt1();
			}
			if (lvl.find("opt2") != std::string::npos) {
				c->set_verbose_opt2();
			}
			if (lvl.find("map") != std::string::npos) {
				c->set_verbose_map();
			}
			if (lvl.find("ir") != std::string::npos) {
				c->set_verbose_ir_gen();
			}
			if (lvl.find("classify") != std::string::npos) {
				c->set_verbose_classify();
			}
			if (lvl.find("code-gen") != std::string::npos) {
				c->set_verbose_code_gen();
			}
			if (lvl.find("reader") != std::string::npos) {
				c->set_verbose_read();
			}
		}
		else if (strcmp(argv[i], "--silent") == 0) {
			silent = true;
		}
		else {
			std::cout << "Error:Unknown input " << argv[i] << std::endl;
			exit(1);
		}
	}

	if (c->get_FIFO_size() == 0) {
		std::cout << "FIFO Size undefined. Abort." << std::endl;
		exit(1);
	}
	if (strcmp(c->get_source_dir(), "") == 0) {
		std::cout << "Source directory undefined. Abort." << std::endl;
		exit(1);
	}
	if (strcmp(c->get_target_dir(), "") == 0) {
		std::cout << "Target directory undefined. Abort." << std::endl;
		exit(1);
	}
	if (strcmp(c->get_network_file(), "") == 0) {
		std::cout << "Network File undefined. Abort." << std::endl;
		exit(1);
	}
	if (c->get_cores() == 0) {
		if (!silent) {
			std::cout << "Number of cores to use not set; Assuming four." << std::endl;
		}
		c->set_cores(4);
	}
	if (!mapping_set) {
		if (!silent) {
			std::cout << "Using mapping strategy all to all (default)." << std::endl;
		}
		c->set_mapping_strategy_all_to_all();
	}
	if (!schedule_set) {
		if (!silent) {
			std::cout << "Using non-preemptive scheduling (default)." << std::endl;
		}
		c->set_sched_non_preemptive();
	}
}


int main(int argc, char* argv[]) {
	Config* c = Config::getInstance();

	parse_command_line_input(argc, argv);

	if (silent) {
		std::cout.setstate(std::ios_base::failbit);
	}

	std::cout << "Network File: " << c->get_network_file() << std::endl;
	std::cout << "CAL Source directory: " << c->get_source_dir() << std::endl;
	std::cout << "Target directory: " << c->get_target_dir() << std::endl;
	std::cout << "Cores: " << c->get_cores() << std::endl;
	std::cout << "FIFO Size: " << c->get_FIFO_size() << std::endl;
	if (c->get_orcc_compat()) {
		std::cout << "ORCC compatibility." << std::endl;
	}
	if (c->get_cmake()) {
		std::cout << "CMake File generation." << std::endl;
	}
	if (c->get_target_language() == Target_Language::c) {
		std::cout << "Generating C code." << std::endl;
	}
	if (c->get_target_language() == Target_Language::cpp) {
		std::cout << "Generating C++ code." << std::endl;
	}
	std::cout << "Scheduling: ";
	if (c->get_sched_non_preemptive()) {
		std::cout << "Non-Preemptive." << std::endl;
	}
	else if (c->get_sched_rr()) {
		std::cout << "Round-Robin." << std::endl;
	}

	std::cout << "Mapping: ";
	if (c->get_mapping_strategy_all_to_all()) {
		std::cout << "All actors to all cores." << std::endl;
	}
	else if (c->get_random_mapping()) {
		std::cout << "Random." << std::endl;
	}
	else if (c->is_map_file()) {
		std::cout << "Read from XML file." << std::endl;
	}

	if (c->get_target_ABI() == Target_ABI::rtos) {
		std::cout << "Generating code for RTOS." << std::endl;
		std::cout << "Mapping Strategy: ";
		if (c->get_rtos_strategy() == RTOS_Strategy::basic) {
			std::cout << "Basic" << std::endl;
		}
		else if (c->get_rtos_strategy() == RTOS_Strategy::counting) {
			std::cout << "Counting" << std::endl;
		}
		else if (c->get_rtos_strategy() == RTOS_Strategy::schedcheck) {
			std::cout << "Schedcheck" << std::endl;
		}
		if (c->get_use_mutex()) {
			std::cout << "Using Mutex to protect notify decision.\n";
		}
		if (c->get_use_atomics()) {
			std::cout << "Using Atomics to protect notify decision.\n";
		}
		if (c->get_deadline() != 0) {
			std::cout << "Deadline: " << c->get_deadline() << std::endl;
		}
		else {
			std::cout << "No deadline set." << std::endl;
		}
		if (c->get_release()!= 0) {
			std::cout << "Release: " << c->get_release() << std::endl;
		}
		else {
			std::cout << "No release time set." << std::endl;
		}
		if (!c->get_globals_prefix().empty()) {
			std::cout << "Globals prefix: " << c->get_globals_prefix() << std::endl;
		}
		if (!c->get_rtos_subdir().empty()) {
			std::cout << "Subdirectory: " << c->get_rtos_subdir() << std::endl;
		}

		if (c->get_use_mutex() && c->get_use_atomics()) {
			std::cout << "Atomics and Mutex cannot be combined." << std::endl;
			exit(54);
		}

		if (c->get_orcc_compat()) {
			std::cout << "ORCC compatibility not supported for RTOS." << std::endl;
			exit(55);
		}
		if (c->get_omp_tasking()) {
			std::cout << "OMP Tasking not supported for RTOS." << std::endl;
			exit(56);
		}
		if (c->get_cmake()) {
			std::cout << "CMake generation not supported for RTOS. Makefile is generated by default." << std::endl;
			exit(57);
		}
	}

	if ((c->get_target_ABI() == Target_ABI::stdc) && (c->get_cores() > 1)) {
		std::cout << "C Standard doesn't provide multi threading...exiting." << std::endl;
		exit(91);
	}
	if ((c->get_target_language() == Target_Language::c) && (c->get_list_scheduling())) {
		std::cout << "List scheduling not supported for C ...exiting." << std::endl;
		exit(92);
	}
	if (c->get_optimize_core_merge() && c->get_optimize_config_merge()) {
		std::cout << "Core merge and config merge optimizations cannot be combined." << std::endl;
		exit(93);
	}

	const auto start{ std::chrono::steady_clock::now() };

	/* Create target directory if it doesn't exist. */
	std::filesystem::create_directory(c->get_target_dir());

	IR::Dataflow_Network* dpn;

	try {
		dpn = Network_Reader::read_network();
		Network_Reader::read_actors(dpn);
	}
	catch (const Network_Reader::Network_Reader_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Network read." << std::endl;

	std::cout << "Actors found: " << dpn->get_actors()->size() << std::endl;
	std::cout << "Actor instances found: " << dpn->get_actor_instances().size() << std::endl;
	std::cout << "Edges found: " << dpn->get_edges().size() << std::endl;
	try {
		for (auto it = dpn->get_actors()->begin(); it != dpn->get_actors()->end(); ++it) {
			(*it)->transform_IR(dpn);
		}
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "IR Transformation done." << std::endl;

	try {
		Dataflow_Analysis::network_analysis(dpn);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Dataflow Analysis done." << std::endl;

	try {
		for (auto it = dpn->get_actors()->begin(); it != dpn->get_actors()->end(); ++it) {
			(*it)->classify_actor();
		}
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Actor Classification done." << std::endl;

	const auto opt_start{ std::chrono::steady_clock::now() };

	Optimization::Optimization_Data_Phase1* opt_data1;
	try {
		opt_data1 = Optimization::optimize_phase1(dpn);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Phase 1 Optimization done." << std::endl;

	Mapping::Mapping_Data* map_data;
	try {
		map_data = Mapping::mapping(dpn);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Actor to PE Mapping done." << std::endl;

	Optimization::Optimization_Data_Phase2* opt_data2;
	try {
		opt_data2 = Optimization::optimize_phase2(dpn);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	const auto opt_end{ std::chrono::steady_clock::now() };
	const std::chrono::duration<double> opt_elapsed{ opt_end - opt_start };

	if (silent) {
		std::cout.clear();
	}

	std::cout << "Optimization took: " << opt_elapsed << std::endl;

	if (silent) {
		std::cout.setstate(std::ios_base::failbit);
	}

	std::cout << "Phase 2 Optimization done." << std::endl;

	try {
		Code_Generation::generate_code(dpn, opt_data1, opt_data2, map_data);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Code Generation done." << std::endl;

	const auto end{ std::chrono::steady_clock::now() };
	const std::chrono::duration<double> elapsed{ end - start };

	if (silent) {
		std::cout.clear();
	}

	std::cout << "Transpilation took: " << elapsed << std::endl;

	return 0;
}