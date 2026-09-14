#pragma once

#include<vector>
#include <cstring>
#include <string>
#include "debug.h"

enum Target_Language
{
    cpp,
    c,
};

enum Target_ABI {
    stdc,
    stdcpp,
    rtos
};

enum RTOS_Strategy {
    basic,
    counting,
    schedcheck,
};

/* Singleton to store the configuration data parsed from command line parameters! */
class Config {
    static Config* instance;
    std::string source_directory;
    std::string target_directory;
    std::string network_file;
    unsigned int FIFO_size;
    unsigned int cores;
    bool orcc_compat{ false };
    bool cmake{ false };
    bool static_alloc{ false };
    Target_Language target_language{ cpp };
    Target_ABI target_ABI{ stdcpp };
    RTOS_Strategy rtos_strat{ basic };
    std::string globals_prefix{};
    std::string rtos_subdir{};
    std::string channel_size_file;
	std::string feedback_edges_file;

    // Mapping strategies
    bool mapping_all_to_all{ false };
    bool mapping_from_file{ false }; //Flag whether mapping_file is valid
    std::string mapping_file;
    bool mapping_level{ false };
    std::string output_nodes_file;
    bool use_outputs_from_file{ false };
    std::string input_nodes_file;
    bool use_inputs_from_file{ false };
    bool random_mapping{ false };


    // Scheduling
    bool topology_sort{ false };
    bool non_preemptive{ false };
    bool list_scheduling{ false };
    bool rr_scheduling{ false };
    unsigned local_sched_loops{ 0 };
    bool limit_local_sched_loops{ false };
    std::string loop_bound_file;

    // RTOS
    bool use_mutex{ false };
    bool use_atomics{ false };
    unsigned release{ 0 };
    unsigned deadline{ 0 };
	unsigned rtos_sched_cycles{ 1 };

    //OpenMP
    bool omp_tasking{ false };

    //Optimization
    bool prune_disconnected{ false };
    bool optimize_scheduling{ false };
    bool optimize_core_merge{ false };
	bool optimize_config_merge{ false };
    bool prolog_epilog_opt{ true };
	std::string merge_config_file;

    //verbose
    bool verbose_read{ false };
    bool verbose_opt1{ false };
    bool verbose_opt2{ false };
    bool verbose_map{ false };
    bool verbose_ir_gen{ false };
    bool verbose_code_gen{ false };
    bool verbose_classify{ false };

    // Private constructor so that no objects can be created.
    Config() {
        source_directory = "";
        target_directory = "";
        network_file = "";
        FIFO_size = 0;
        cores = 0;
    }

public:
    static Config* getInstance() {
        if (!instance) {
            instance = new Config;
        }
        return instance;
    }

    const char* get_source_dir() {
        return this->source_directory.c_str();
    }

    void set_source_dir(const char *src) {
        this->source_directory = src;
    }

    const char* get_target_dir() {
        return this->target_directory.c_str();
    }

    void set_target_dir(const char* target) {
        this->target_directory = target;
    }

    const char* get_network_file() {
        return this->network_file.c_str();
    }

    void set_network_file(const char* n) {
        this->network_file = n;
    }

    unsigned int get_FIFO_size() {
        return this->FIFO_size;
    }

    void set_FIFO_size(unsigned int n) {
        this->FIFO_size = n;
    }

    unsigned int get_cores() {
        return this->cores;
    }

    void set_cores(unsigned int n) {
        this->cores = n;
    }

    void set_static_alloc(void) {
        this->static_alloc = true;
    }

    bool get_static_alloc(void) {
        return this->static_alloc;
    }

    void set_globals_prefix(std::string s) {
        globals_prefix = s;
    }
    std::string get_globals_prefix(void) {
        return globals_prefix;
    }

    void set_target_language(Target_Language t) {
        target_language = t;
    }

    Target_Language get_target_language(void) {
        return target_language;
    }

    void set_target_ABI(Target_ABI t) {
        target_ABI = t;
    }

    Target_ABI get_target_ABI(void) {
        return target_ABI;
    }

    void set_mapping_strategy_all_to_all(void) {
        if (mapping_from_file != true) {
            mapping_all_to_all = true;
        }
    }

    bool get_mapping_strategy_all_to_all(void) {
        return mapping_all_to_all;
    }

    void set_orcc_compat(void) {
        orcc_compat = true;
    }

    bool get_orcc_compat(void) {
        return orcc_compat;
    }

    void set_topology_sort(void) {
        topology_sort = true;
    }

    bool get_topology_sort(void) {
        return topology_sort;
    }

    void set_omp_tasking(void) {
        omp_tasking = true;
    }

    bool get_omp_tasking(void) {
        return omp_tasking;
    }

    void set_sched_non_preemptive(void) {
        non_preemptive = true;
    }
    bool get_sched_non_preemptive(void) {
        return non_preemptive;
    }

    void set_sched_rr(void) {
        rr_scheduling = true;
    }
    bool get_sched_rr(void) {
        return rr_scheduling;
    }

    void set_bound_local_sched_loops(unsigned n) {
        local_sched_loops = n;
        limit_local_sched_loops = true;
    }

    bool get_bound_local_sched_loops(void) {
        return limit_local_sched_loops;
    }
    unsigned get_local_sched_loop_num(void) {
        return local_sched_loops;
    }

    void set_bound_sched_loops_file(std::string s) {
        loop_bound_file = s;
        limit_local_sched_loops = true;
    }
    std::string get_bound_sched_loops_file(void) {
        return loop_bound_file;
    }

    void set_prune_disconnected(void) {
        prune_disconnected = true;
    }

    bool get_prune_disconnected(void) {
        return prune_disconnected;
    }

    void set_optimize_scheduling(void) {
        optimize_scheduling = true;
    }

    bool get_optimize_scheduling(void) {
        return optimize_scheduling;
    }

    void set_list_scheduling(void) {
        list_scheduling = true;
    }

    bool get_list_scheduling(void) {
        return list_scheduling;
    }

    void set_mapping_file(std::string f) {
        mapping_all_to_all = false;
        mapping_level = false;
        mapping_from_file = true;
        mapping_file = f;
    }

    bool is_map_file(void) {
        return mapping_from_file;
    }

    std::string get_mapping_file(void) {
        return mapping_file;
    }

    void set_cmake(void) {
        cmake = true;
    }

    bool get_cmake(void) {
        return cmake;
    }

    void set_mapping_level(void) {
        mapping_level = true;
    }
    bool get_mapping_level(void) {
        return mapping_level;
    }

    void set_output_nodes_file(std::string f) {
        output_nodes_file = f;
        use_outputs_from_file = true;
    }
    std::string get_output_nodes_file(void) {
        return output_nodes_file;
    }
    bool get_use_outputs_from_file(void) {
        return use_outputs_from_file;
    }

    void set_input_nodes_file(std::string f) {
        input_nodes_file = f;
        use_inputs_from_file = true;
    }
    std::string get_input_nodes_file(void) {
        return input_nodes_file;
    }
    bool get_use_inputs_from_file(void) {
        return use_inputs_from_file;
    }

    void set_random_mapping(void) {
        random_mapping = true;
    }
    bool get_random_mapping(void) {
        return random_mapping;
    }

    void set_verbose_read(void) {
        verbose_read = true;
    }
    bool get_verbose_read(void) {
        return verbose_read;
    }

    void set_verbose_opt1(void) {
        verbose_opt1 = true;
    }
    bool get_verbose_opt1(void) {
        return verbose_opt1;
    }

    void set_verbose_opt2(void) {
        verbose_opt2 = true;
    }
    bool get_verbose_opt2(void) {
        return verbose_opt2;
    }

    void set_verbose_map(void) {
        verbose_map = true;
    }
    bool get_verbose_map(void) {
        return verbose_map;
    }

    void set_verbose_ir_gen(void) {
        verbose_ir_gen = true;
    }
    bool get_verbose_ir_gen(void) {
        return verbose_ir_gen;
    }

    void set_verbose_code_gen(void) {
        verbose_code_gen = true;
    }
    bool get_verbose_code_gen(void) {
        return verbose_code_gen;
    }

    void set_verbose_classify(void) {
        verbose_classify = true;
    }
    bool get_verbose_classify(void) {
        return verbose_classify;
    }

    void set_optimize_core_merge(void) {
        optimize_core_merge = true;
    }
    bool get_optimize_core_merge(void) {
        return optimize_core_merge;
    }

    void set_optimize_config_merge(void) {
        optimize_config_merge = true;
	}
    bool get_optimize_config_merge(void) {
        return optimize_config_merge;
	}

    void clear_prolog_epilog_opt(void) {
        prolog_epilog_opt = false;
    }
    bool get_prolog_epilog_opt(void) {
        return prolog_epilog_opt;
    }

    std::string get_merge_config_file(void) {
        return merge_config_file;
	}
    void set_merge_config_file(std::string f) {
        merge_config_file = f;
    }

    void set_use_mutex(void) {
        use_mutex = true;
    }
    bool get_use_mutex(void) {
        return use_mutex;
    }

    void set_use_atomics(void) {
        use_atomics = true;
    }
    bool get_use_atomics(void) {
        return use_atomics;
    }

    RTOS_Strategy get_rtos_strategy(void) {
        return rtos_strat;
    }
    void set_rtos_strategy(RTOS_Strategy s) {
        rtos_strat = s;
    }

    void set_release(unsigned r) {
        release = r;
    }
    unsigned get_release(void) {
        return release;
    }

    void set_deadline(unsigned d) {
        deadline = d;
    }
    unsigned get_deadline(void) {
        return deadline;
    }

    void set_rtos_subdir(std::string s) {
        rtos_subdir = s;
    }
    std::string get_rtos_subdir(void) {
        return rtos_subdir;
    }

    void set_rtos_sched_cycles(unsigned c) {
        /* 0 would divide the scheduling loop bound by zero */
        rtos_sched_cycles = (c == 0) ? 1 : c;
	}
    unsigned get_rtos_sched_cycles(void) {
        return rtos_sched_cycles;
	}

    void set_channel_size_file(std::string f) {
        channel_size_file = f;
	}
    std::string get_channel_size_file(void) {
        return channel_size_file;
	}

    void set_feedback_edges_file(std::string f) {
        feedback_edges_file = f;
    }
    std::string get_feedback_edges_file(void) {
        return feedback_edges_file;
	}
};