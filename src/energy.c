#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "energy.h"

#define RAPL_PKG_ENERGY_FILE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/energy_uj"
#define RAPL_CORE_ENERGY_FILE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj"
#define RAPL_UNCORE_ENERGY_FILE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:1/energy_uj"
#define RAPL_DRAM_ENERGY_FILE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:2/energy_uj"
#define RAPL_PSYS_ENERGY_FILE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:1/energy_uj"
#define RAPL_MAX_RANGE "/sys/devices/virtual/powercap/intel-rapl/intel-rapl:0/max_energy_range_uj"

// potentially more packages which one to check, all?
// pp0 + pp1 <= pkg, dram independent, pkg + dram <= psys includes all

static uint64_t max_range; 

// 0->pkg, 1->cores, 2->uncore, 3->dram, 4->psys, seperate functions more energy efficient?
uint64_t read_energy(int domain) {
    FILE *fp;
    uint64_t energy_microjoules;

    switch (domain) {
        case 0: // package
            fp = fopen(RAPL_PKG_ENERGY_FILE, "r");
            break;
        case 1: // cores
            fp = fopen(RAPL_CORE_ENERGY_FILE, "r");
            break;
        case 2: // uncore
            fp = fopen(RAPL_UNCORE_ENERGY_FILE, "r");
            break;
        case 3: // dram
            fp = fopen(RAPL_DRAM_ENERGY_FILE, "r");
            break;
        case 4: // psys
            fp = fopen(RAPL_PSYS_ENERGY_FILE, "r");
            break;
        default:
            return -1;
    }
    if (fp == NULL) {
        perror("Couldn't get filehandle (read_energy). Need sudo");
        return -1;
    }
    fscanf(fp, "%"PRIu64, &energy_microjoules);
    fclose(fp);

    return energy_microjoules;
}


// msr overflow checking and fixing
uint64_t check_overflow(uint64_t before, uint64_t after) {
    if (before > after) {
        return (after + max_range - before);
    }
    return after - before;
}

// check available rapl domains, packages, set max_range overflow, 
int init_rapl() {
    // max range
    FILE *fp;
    fp = fopen(RAPL_MAX_RANGE, "r");
    if (fp == NULL) {
        perror("Couldn't get filehandle (read_energy). Need sudo");
        return -1;
    }
    fscanf(fp, "%"PRIu64, &max_range);
    fclose(fp);

    // TODO ///
    // check available rapl domains, packages
    // TODO ///

}

// use statistics to estimate consumed energy
uint64_t estimate_energy(unsigned long cputime, long rss, long io_op) {
    uint64_t energy_microj;

    // TODO ///
    // estimate energy
    // TODO ///

    return 0;
    return energy_microj;
}

// different events for different GPUs
uint64_t read_perf_gpu_energy() {

    // TODO ///
    // open perf events
    // TODO ///

    return 0;
}
