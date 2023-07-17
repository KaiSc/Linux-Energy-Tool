#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
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

static long long max_range; // in microjoules
static long long idle_consumption; // in microjoules
static long long idle_min; // in microjoules

// 0->pkg, 1->cores, 2->uncore, 3->dram, 4->psys, seperate functions more energy efficient?
long long read_energy(int domain) {
    FILE *fp;
    long long energy_microjoules;

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
            return 0;
    }
    if (fp == NULL) {
        return 0;
    }
    fscanf(fp, "%lld", &energy_microjoules);
    fclose(fp);

    return energy_microjoules;
}

// msr overflow checking and fixing
long long check_overflow(long long before, long long after) {
    if (before > after) {
        return (after + max_range - before);
    }
    return after - before;
}

// check available rapl domains, packages, set max_range overflow, 
int init_rapl() {
    // Check if config_idle.txt file exists
    if (access("config_idle.txt", F_OK) != -1) {
        // File exists, read its contents
        FILE *fp = fopen("config_idle.txt", "r");
        if (fp == NULL) {
            perror("Couldn't open config_idle.txt file");
            return -1;
        }
        fscanf(fp, "%lld %lld", &idle_consumption, &idle_min);
        printf("Idle power config (microjoules per 1 second): %lld\n", idle_consumption);
        fclose(fp);
    } else {
        // File does not exist
        printf("No idle power config file present, run with -i on idle system\n");
    }

    // max range
    FILE *fp;
    fp = fopen(RAPL_MAX_RANGE, "r");
    if (fp == NULL) {
        perror("Couldn't get filehandle (read_energy). Need sudo");
        return -1;
    }
    fscanf(fp, "%llu", &max_range);
    fclose(fp);
    
    return 0;
}

// Use statistics to estimate consumed energy in microjoules
long long estimate_energy_cycles(long long cpu_cycles, long long cpu_cycles_proc,
        long long energy_interval, double time)
{
    long long energy_estimation = 0;
    double idle_contribution = idle_consumption * time;
    // if idle avg is higher than measured use minium val
    if (idle_contribution > energy_interval) {
        idle_contribution = idle_min * time;
        // if min value is still higher than measured return 0
        if (idle_contribution > energy_interval) {
            return 0;
        }
    }
    // compute fraction
    if (cpu_cycles_proc==0) {
        //printf("sys_cycles %lld - proc_cycles %lld - fraction %.6f \n", cpu_cycles, cpu_cycles_proc, 0.0);
        return 0;
    }
    double cpu_cycles_frac = (double) cpu_cycles_proc / cpu_cycles;
    //printf("sys_cycles %lld - proc_cycles %lld - fraction %.6f \n", cpu_cycles, cpu_cycles_proc, cpu_cycles_frac);
    energy_estimation = cpu_cycles_frac * (energy_interval - idle_contribution);
    return energy_estimation;
}

/*

long long estimate_energy_cputime(unsigned long cputime, unsigned long cputime_proc,
        long long energy_interval) 
{
    double energy_estimation = 0;
    // compute fraction
    if (cputime_proc==0) {return 0;}
    double cputime_frac = (double) cputime_proc / cputime;
    printf("cputime %lu --- cputime_proc %lu --- frac %.6f \n", cputime, cputime_proc, cputime_frac);
    energy_estimation = cputime_frac * ((double) energy_interval - idle_consumption);
    return energy_estimation;
}

long long estimate_energy(unsigned long cputime, long rss, long io_op, long long cpu_cycles, 
        unsigned long cputime_proc, long rss_proc, long io_op_proc, long long cpu_cycles_proc,
        long long energy_interval) 
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
