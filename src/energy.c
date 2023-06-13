#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "energy.h"

// TODO hardcoded powercap energy paths
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

// Use statistics to estimate consumed energy
// TODO
// - better estimation, differntiate pkg/ram if supported
// - deduct idle power consumption 
double estimate_energy_cputime(unsigned long cputime, unsigned long cputime_proc,
        uint64_t energy_interval) 
{
    double idle_consumption = 0;
    double energy_estimation = 0;
    // compute fraction
    if (cputime_proc==0) {return 0;}
    double cputime_frac = (double) cputime_proc / cputime;
    printf("cputime %lu --- cputime_proc %lu --- frac %.6f \n", cputime, cputime_proc, cputime_frac);
    energy_estimation = cputime_frac * ((double) energy_interval - idle_consumption);
    return energy_estimation;
}

double estimate_energy_cycles(unsigned long cpu_cycles, long long cpu_cycles_proc,
        uint64_t energy_interval)
{
    double idle_consumption = 0;
    double energy_estimation = 0;
    // compute fraction
    if (cpu_cycles_proc==0) {return 0;}
    double cpu_cycles_frac = (double) cpu_cycles_proc / cpu_cycles;
    printf("cycles %lu --- cycles_proc %lld --- frac %.6f \n", cpu_cycles, cpu_cycles_proc, cpu_cycles_frac);
    energy_estimation = cpu_cycles_frac * ((double) energy_interval - idle_consumption);
    return energy_estimation;
}

/*
double estimate_energy(unsigned long cputime, long rss, long io_op, long long cpu_cycles, 
        unsigned long cputime_proc, long rss_proc, long io_op_proc, long long cpu_cycles_proc,
        uint64_t energy_interval) 
{
    double energy_estimation = 0;
    // compute fractions
    double cputime_frac = (double) cputime_proc / cputime;
    double rss_frac = (double) rss_proc / rss;
    double io_op_frac = (double) io_op_proc / io_op;
    double cpu_cycles_frac = (double) cpu_cycles_proc / cpu_cycles;

    energy_estimation = cputime_frac * energy_interval * 0.9 + rss_frac * energy_interval *0.05
                        + io_op_frac * energy_interval *0.05;

    return energy_estimation;
}
// */
