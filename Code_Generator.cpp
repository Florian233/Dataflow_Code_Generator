#include <iostream>
#include <memory>
#include <sstream>
#include <filesystem>
#include "Config/config.h"
#include "Config/debug.h"
#include "IR/Dataflow_Network.hpp"
#include "Reader/reader.hpp"
#include "Actor_Classification/Actor_Classification.hpp"
#include "Optimization_Phase1/Optimization_Phase1.hpp"
#include "Optimization_Phase2/Optimization_Phase2.hpp"
#include "Mapping/Mapping.hpp"
#include "Code_Generation/Code_Generation.hpp"
using namespace std;

Config* Config::instance = 0;

void parse_command_line_input(int argc, char* argv[]) {
	Config* c = c->getInstance();
	bool mapping_set{ false };
	bool schedule_set{ false };
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0) {
			std::cout << "\nUsage: %s [options]\n"
				"\nCommon arguments:\n"
				"	-h                 Print this message.\n"
				"	-w <directory>     Specify the directory to write the output to.\n"
				"	-d <directory>     Specify the directory of the RVC-CAL sources.\n"
				"	-n <file>          Specify the top network that shall be converted.\n"
				"	--orcc			   ORCC compatibility, required to use ORCC projects.\n"
				"   --cmake            Generate CMake File for the generated code.\n"
				"\nCommunication Channels:\n"
				"	-s <number>        Specify the default size of the FIFOs.\n"
				"\nOpenMP:\n"
				"	--omp_tasking      Use OpenMP tasking for the parallel execution of the global schedulers.\n"
				"\nMapping:\n"
				"	-c <number>        Specify the number of cores to use.\n"
				"	--map=all\n"               
				"                 all: Map all actor instances to all cores (default).\n"
				"   --map_file <file>  Uses the mapping from <file>, ignores -c and --map.\n"
				"\nScheduling:\n"
				"   --topology_sort    Use topology sorted list for scheduling.\n"
				"   --schedule=non_preemptive\n"
				"                 non_preemtive: Use non-preemptive scheduling strategy (default).\n"
				"   --list_schedule    Use a list for scheduling instead of hard-coded scheduler.\n"
				"\nOptimizations:\n"
				"	--prune_unconnected Remove unconnected channels from actors, otherwise they are set to NULL.\n";
			exit(0);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			c->set_target_dir(argv[++i]);
		}
		else if (strcmp(argv[i], "-s") == 0) {
			c->set_FIFO_size(static_cast<unsigned int>(atoi(argv[++i])));
		}
		else if (strcmp(argv[i], "-d") == 0) {
			c->set_source_dir(argv[++i]);
		}
		else if (strcmp(argv[i], "-n") == 0) {
			c->set_network_file(argv[++i]);
		}
		else if (strcmp(argv[i], "-c") == 0) {
			c->set_cores(static_cast<unsigned int>(atoi(argv[++i])));
		}
		else if (strcmp(argv[i], "--orcc") == 0) {
			c->set_orcc_compat();
		}
		else if (strcmp(argv[i], "--cmake") == 0) {
			c->set_cmake();
		}
		else if (strcmp(argv[i], "--map_file") == 0) {
			c->set_mapping_file(argv[++i]);
		}
		else if (strncmp(argv[i], "--map=", 6) == 0) {
			if (strcmp(argv[i] + 6, "all") == 0) {
				c->set_mapping_strategy_all_to_all();
				mapping_set = true;
			}
			else {
				std::cout << "Error: Unknown mapping strategy " << argv[i] << std::endl;
				exit(1);
			}
		}
		else if (strncmp(argv[i], "--schedule=", 11) == 0) {
			if (strcmp(argv[i] + 11, "non_preemptive") == 0) {
				c->set_non_preemptive();
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
		else if (strcmp(argv[i], "--omp_tasking") == 0) {
			c->set_omp_tasking();
		}
		else if (strcmp(argv[i], "--prune_unconnected") == 0) {
			c->set_prune_disconnected();
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
		std::cout << "Number of cores to use not set; Assuming four." << std::endl;
		c->set_cores(4);
	}
	if (!mapping_set) {
		std::cout << "Using mapping strategy all to all (default)." << std::endl;
		c->set_mapping_strategy_all_to_all();
	}
	if (!schedule_set) {
		std::cout << "Using non-preemptive scheduling (default)." << std::endl;
		c->set_non_preemptive();
	}
}


int main(int argc, char* argv[]) {

	parse_command_line_input(argc, argv);

	Config *c = c->getInstance();

	std::cout << "Network File: " << c->get_network_file() << std::endl;
	std::cout << "CAL Source directory: " << c->get_source_dir() << std::endl;
	std::cout << "Target directory: " << c->get_target_dir() << std::endl;
	std::cout << "Cores: " << c->get_cores() << std::endl;
	std::cout << "FIFO Size: " << c->get_FIFO_size() << std::endl;
	if (c->get_orcc_compat()) {
		std::cout << "ORCC compatibility." << std::endl;
	}
	std::cout << "Scheduling: ";
	if (c->get_non_preemptive()) {
		std::cout << "Non-Preemptive." << std::endl;
	}
	std::cout << "Mapping: ";
	if (c->get_mapping_strategy_all_to_all()) {
		std::cout << "All actors to all cores." << std::endl;
	}

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
			(*it)->transform_IR();
		}
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "IR Transformation done." << std::endl;

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

	std::cout << "Phase 2 Optimization done." << std::endl;

	try {
		Code_Generation::generate_code(dpn, opt_data1, opt_data2, map_data);
	}
	catch (const Converter_Exception& e) {
		std::cerr << e.what();
		exit(1);
	}

	std::cout << "Code Generation done." << std::endl;

	return 0;
}


