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

    // Mapping strategies
    bool mapping_all_to_all{ false };
    bool mapping_from_file{ false }; //Flag whether mapping_file is valid
    std::string mapping_file;

    // Scheduling
    bool topology_sort{ false };
    bool non_preemptive{ false };
    bool list_scheduling{ false };

    //OpenMP
    bool omp_tasking{ false };

    //Optimization
    bool prune_disconnected{ false };

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

    void set_non_preemptive(void) {
        non_preemptive = true;
    }

    bool get_non_preemptive(void) {
        return non_preemptive;
    }

    void set_prune_disconnected(void) {
        prune_disconnected = true;
    }

    bool get_prune_disconnected(void) {
        return prune_disconnected;
    }

    void set_list_scheduling(void) {
        list_scheduling = true;
    }

    bool get_list_scheduling(void) {
        return list_scheduling;
    }

    void set_mapping_file(std::string f) {
        mapping_all_to_all = false;
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
};