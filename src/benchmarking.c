#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include "perf_events.h"


#define cgroup_path "/sys/fs/cgroup/benchmarking/"

struct cgroup_stats { 
    unsigned long long cputime; // in microseconds
    long long maxRSS; // in bytes
    unsigned long io_op;
    unsigned long long cycles;
    long long estimated_energy; // in microjoules
};


static int max_cpus = 0;
static int *cgroup_perf_fds;

int init_benchmarking() {
    max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    cgroup_perf_fds = malloc(sizeof(int) * max_cpus);
}

int reset_cgroup() {
    int status = rmdir(cgroup_path);
    status = mkdir(cgroup_path, 0777);

    int fd = open(cgroup_path, O_RDONLY);
    for (int i = 0; i < max_cpus; i++)
    {
        cgroup_perf_fds[i] = setUpProcCycles_cgroup(fd, i);
    }
    close(fd);

    return 0;
}

int close_cgroup() {
    for (int i = 0; i < max_cpus; i++)
    {
        close(cgroup_perf_fds[i]);
    }
    int status = rmdir(cgroup_path);
    return 0;
}

int read_cgroup_stats(struct cgroup_stats *cg_stats) {
    FILE *fp;
    cg_stats->cputime = 0;
    cg_stats->maxRSS = 0;
    cg_stats->io_op = 0;
    cg_stats->cycles = 0;
    cg_stats->estimated_energy = 0;

    // Cgroup Cycles
    long long cpu_cycles = 0;
    for (int i = 0; i < max_cpus; i++)
    {
        cpu_cycles += readInterval(cgroup_perf_fds[i]);
        close(cgroup_perf_fds[i]);
    }
    cg_stats->cycles = cpu_cycles;

    // CPU time
    fp = fopen("/sys/fs/cgroup/benchmarking/cpu.stat", "r");
    if (fp == NULL) {
        perror("Couldn't open /cpu.stat file");
        return -1;
    }
    // Read the cpu.stats file
    fscanf(fp, "%*s %llu", &cg_stats->cputime);
    fclose(fp);

    // Peak Memory
    fp = fopen("/sys/fs/cgroup/benchmarking/memory.peak", "r");
    if (fp == NULL) {
        perror("Couldn't open /memory.peak file");
        return -1;
    }
    fscanf(fp, "%lld", &cg_stats->maxRSS);
    fclose(fp);

    // IO-stats
    fp = fopen("/sys/fs/cgroup/benchmarking/io.stat", "r");
    if (fp == NULL) {
        perror("Couldn't open /io.stat file");
        return -1;
    }
    // Read the io.stat file
    char line[256];
    unsigned long long rbytes = 0, wbytes = 0, rios = 0, wios = 0, ios = 0;
    if (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%*d:%*d rbytes=%llu wbytes=%llu rios=%llu wios=%llu", &rbytes, &wbytes, &rios, &wios) == 4) {
            /*printf("Container Read Bytes: %llu\n", rbytes);
            printf("Container Write Bytes: %llu\n", wbytes);
            printf("Container Read I/Os: %llu\n", rios);
            printf("Container Write I/Os: %llu\n", wios);*/
        }
    }
    fclose(fp);
    ios = rios + wios;
    cg_stats->io_op = ios;

    return 0;
}

/*
int main(int argc, char const *argv[])
{
    init_benchmarking();
    reset_cgroup();
    struct cgroup_stats cg;
    sleep(5);
    read_cgroup_stats(&cg);
    printf("CPU time in microseconds: %llu \n", cg.cputime);
    printf("maxRSS in bytes: %lld \n", cg.maxRSS);
    printf("I/O-operations: %lu \n", cg.io_op);
    printf("CPU Cycles: %llu \n", cg.cycles);
    reset_cgroup();
    read_cgroup_stats(&cg);
    printf("CPU time in microseconds: %llu \n", cg.cputime);
    printf("maxRSS in bytes: %lld \n", cg.maxRSS);
    printf("I/O-operations: %lu \n", cg.io_op);
    printf("CPU Cycles: %llu \n", cg.cycles);

    return 0;
}
*/