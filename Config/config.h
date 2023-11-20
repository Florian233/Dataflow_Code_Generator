#pragma once

#include<vector>
#include <cstring>
#include <string>
#include "debug.h"

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

    // Mapping strategies
    bool mapping_all_to_all{ false };
    bool mapping_from_file{ false }; //Flag whether mapping_file is valid
    std::string mapping_file;
    bool mapping_level{ false };
    bool mapping_weights{ false };
    bool mapping_connected{ false };
    std::string output_nodes_file;
    bool use_outputs_from_file{ false };
    std::string input_nodes_file;
    bool use_inputs_from_file{ false };
    std::string node_weights_file;
    bool use_weights_from_file{ false };
    bool balanced_mapping{ false };
    bool est_mapping{ false }; /* Earliest start time */
    bool lft_mapping{ false }; /* Latest finish time */
    bool rr_mapping{ false };
    bool random_mapping{ false };


    // Scheduling
    bool topology_sort{ false };
    bool non_preemptive{ false };
    bool list_scheduling{ false };
    bool rr_scheduling{ false };
    unsigned local_sched_loops{ 0 };
    bool limit_local_sched_loops{ false };

    //OpenMP
    bool omp_tasking{ false };

    //Optimization
    bool prune_disconnected{ false };
    bool optimize_scheduling{ false };

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
        mapping_connected = false;
        mapping_level = false;
        mapping_weights = false;
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

    void set_mapping_weights(void) {
        mapping_weights = true;
    }
    bool get_mapping_weights(void) {
        return mapping_weights;
    }

    void set_mapping_connected(void) {
        mapping_connected = true;
    }
    bool get_mapping_connected(void) {
        return mapping_connected;
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

    void set_node_weights_file(std::string f) {
        node_weights_file = f;
        use_weights_from_file = true;
    }
    std::string get_node_weights_file(void) {
        return node_weights_file;
    }
    bool get_use_weights_from_file(void) {
        return use_weights_from_file;
    }

    void set_lft_mapping(void) {
        lft_mapping = true;
    }
    bool get_lft_mapping(void) {
        return lft_mapping;
    }

    void set_est_mapping(void) {
        est_mapping = true;
    }
    bool get_est_mapping(void) {
        return est_mapping;
    }

    void set_balanced_mapping(void) {
        balanced_mapping = true;
    }
    bool get_balanced_mapping(void) {
        return balanced_mapping;
    }

    void set_rr_mapping(void) {
        rr_mapping = true;
    }
    bool get_rr_mapping(void) {
        return rr_mapping;
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
};