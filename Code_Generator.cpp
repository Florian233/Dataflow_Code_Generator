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
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			std::cout << "\nUsage: %s [options]\n"
				"\nCommon arguments:\n"
				"	-h                 Print this message.\n"
				"	-w <directory>     Specify the directory to write the output to.\n"
				"	-d <directory>     Specify the directory of the RVC-CAL sources.\n"
				"	-n <file>          Specify the top network that shall be converted.\n"
				"	--orcc			   ORCC compatibility, required to use ORCC projects.\n"
				"   --cmake            Generate CMake File for the generated code.\n"
				"   --static_alloc      Make allocations static, don't use new operator.\n"
				"\nCommunication Channels:\n"
				"	-s <number>        Specify the default size of the FIFOs.\n"
				"\nOpenMP:\n"
				"	--omp_tasking      Use OpenMP tasking for the parallel execution of the global schedulers.\n"
				"\nMapping:\n"
				"	-c <number>        Specify the number of cores to use.\n"
				"	--map=(all|(weighted-)*lft|random)\n"
				"                 all: Map all actor instances to all cores (default).\n"
				"                 lft: Interpret network as a scheduling graph, schedule/map nodes with earliest last-finish-time first in round-robin fashin to the cores.\n"
				"                 balanced-lft: As lft but try to balance the nodes equally amoung the cores, fill one core before advancing to the next.\n"
				"                 balanced-round-robin-lft: As balanced-lft but assign nodes to cores in round-robin fashion for cores. \n"
				"                 level-range-lft: assign all nodes with a last-finish-time in a certain range to each core.\n"
				"                 connected-lft: Like balanced-lft but assign connected nodes preferably to the same core, pick core with least unmapped predecessors.\n"
				"                 connected-round-robin-balanced-lft: Like balanced-round-robin-lft but assigned conntected nodes preferably to the same core.\n"
				"                                                     Pick core that has closes LFT. Advance to next core after each path until all actors are mapped.\n"
				"                 connected-balanced-lft: Like connected-round-robin-balanced-lft but fill one core before advancing to the next."
				"   --map_file <file>  Uses the mapping from <file>, ignores -c and --map.\n"
				"   --output_nodes_file <file> Provide output nodes as a file, avoids autodetection of output nodes as this might fail due to feedback loops.\n"
				"   --input_nodes_file <file> Provide input nodes as a file, avoids autodetection of input nodes as this might fail due to feedback loops.\n"
				"   --map_weights <file> Read weights for each actor for <file>, by defaults the weights are number of consumed and produced tokens.\n"
				"\nScheduling:\n"
				"   --topology_sort    Use topology sorted list for scheduling.\n"
				"   --schedule=(non_preemptive|round_robin)\n"
				"                 non_preemtive: Use non-preemptive scheduling strategy (default).\n"
				"                 round_robin: Use round-robin scheduling strategy.\n"
				"   --list_schedule    Use a list for scheduling instead of hard-coded scheduler.\n"
				"   --bound_sched <number>      Use bound loops for local scheduling with given number of tries.\n"
				"\nOptimizations:\n"
				"	--prune_unconnected Remove unconnected channels from actors, otherwise they are set to NULL.\n"
				"   --opt_sched         Use optimized local scheduling.\n"
				"\nVerbosity:\n"
				"   --verbose=(all, reader, ir, classify, opt1, opt2, map, code-gen)";
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
		else if (strcmp(argv[i], "--static_alloc") == 0) {
			c->set_static_alloc();
		}
		else if (strcmp(argv[i], "--map_file") == 0) {
			c->set_mapping_file(argv[++i]);
			mapping_set = true;
		}
		else if (strcmp(argv[i], "--output_nodes_file") == 0) {
			c->set_output_nodes_file(argv[++i]);
		}
		else if (strcmp(argv[i], "--input_nodes_file") == 0) {
			c->set_input_nodes_file(argv[++i]);
		}
		else if (strcmp(argv[i], "--map_weights") == 0) {
			c->set_node_weights_file(argv[++i]);
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
			else if (strat.ends_with("lft")) {
				if (strat.starts_with("weighted-")) {
					c->set_mapping_weights();
					strat.erase(0, strat.find("-") + 1);
				}
				if (strat == "lft") {
					c->set_lft_mapping();
					mapping_set = true;
				}
				else if (strat == "balanced-lft") {
					c->set_balanced_mapping();
					c->set_lft_mapping();
					mapping_set = true;
				}
				else if (strat == "balanced-round-robin-lft") {
					c->set_balanced_mapping();
					c->set_lft_mapping();
					c->set_rr_mapping();
					mapping_set = true;
				}
				else if (strat == "level-range-lft") {
					c->set_mapping_level();
					c->set_lft_mapping();
					mapping_set = true;
				}
				else if (strat == "connected-lft") {
					c->set_mapping_connected();
					c->set_lft_mapping();
					mapping_set = true;
				}
				else if (strat == "connected-balanced-lft") {
					c->set_mapping_connected();
					c->set_balanced_mapping();
					c->set_lft_mapping();
					mapping_set = true;
				}
				else if (strat == "connected-round-robin-balanced-lft") {
					c->set_balanced_mapping();
					c->set_mapping_connected();
					c->set_lft_mapping();
					c->set_rr_mapping();
					mapping_set = true;
				}
				else {
					std::cout << "Error: Unknown mapping strategy " << argv[i] << std::endl;
					exit(1);
				}
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
			c->set_bound_local_sched_loops(static_cast<unsigned int>(atoi(argv[++i])));
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
		c->set_sched_non_preemptive();
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
	if (c->get_cmake()) {
		std::cout << "CMake File generation." << std::endl;
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
	else if (c->get_lft_mapping()) {
		if (c->get_mapping_weights()) {
			std::cout << "weighted-";
		}
		if (c->get_mapping_connected() && c->get_balanced_mapping() && c->get_rr_mapping()) {
			std::cout << "connected-round-robin-balanced-lft" << std::endl;
		}
		else if (c->get_mapping_connected() && c->get_balanced_mapping()) {
			std::cout << "connected-balanced-lft" << std::endl;
		}
		else if (c->get_mapping_connected()) {
			std::cout << "connected-lft" << std::endl;
		}
		else if (c->get_balanced_mapping() && c->get_rr_mapping()) {
			std::cout << "balanced-round-robin-lft" << std::endl;
		}
		else if (c->get_balanced_mapping()) {
			std::cout << "balanced-lft" << std::endl;
		}
		else if (c->get_mapping_level()) {
			std::cout << "level-range-lft" << std::endl;
		}
		else {
			std::cout << "lft" << std::endl;
		}
	}
	else if (c->is_map_file()) {
		std::cout << "Read from XML file." << std::endl;
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


