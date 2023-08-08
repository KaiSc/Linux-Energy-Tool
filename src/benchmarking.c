#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysinfo.h>
#include "perf_events.h"


#define cgroup_path "/sys/fs/cgroup/benchmarking"

struct cgroup_stats { 
    unsigned long long cputime; // in microseconds
    long long maxRSS; // in bytes
    unsigned long io_op;
    unsigned long long cycles;
    long long estimated_energy; // in microjoules
    long long r_bytes; // read disk bytes
    long long w_bytes; // written disk bytes
};


static int max_cpus = 0;
static int *cgroup_perf_fds;
static char cgroup_path_id[220];
static char cgroup_path_id_cpu[256];
static char cgroup_path_id_mem[256];
static char cgroup_path_id_io[256];
pid_t cgroup_id;


int init_benchmarking() {
    max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    cgroup_perf_fds = malloc(sizeof(int) * max_cpus);
    // Add pid to cgroup path in case multiple instances running at same time
    cgroup_id = getpid();
    // Create the cgroup_path_id strings
    snprintf(cgroup_path_id, 220, "%s_%d", cgroup_path, cgroup_id);
    snprintf(cgroup_path_id_cpu, 256, "%s%s", cgroup_path_id, "/cpu.stat");
    snprintf(cgroup_path_id_mem, 256, "%s%s", cgroup_path_id, "/memory.peak");
    snprintf(cgroup_path_id_io, 256, "%s%s", cgroup_path_id, "/io.stat");

}

int reset_cgroup() {
    int status = rmdir(cgroup_path_id);
    status = mkdir(cgroup_path_id, 0777);

    int fd = open(cgroup_path_id, O_RDONLY);
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
    int status = rmdir(cgroup_path_id);
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
    fp = fopen(cgroup_path_id_cpu, "r");
    if (fp == NULL) {
        perror("Couldn't open /cpu.stat file");
        return -1;
    }
    // Read the cpu.stats file
    fscanf(fp, "%*s %llu", &cg_stats->cputime);
    fclose(fp);

    // Peak Memory
    fp = fopen(cgroup_path_id_mem, "r");
    if (fp == NULL) {
        perror("Couldn't open /memory.peak file");
        return -1;
    }
    fscanf(fp, "%lld", &cg_stats->maxRSS);
    fclose(fp);

    // IO-stats
    fp = fopen(cgroup_path_id_io, "r");
    if (fp == NULL) {
        perror("Couldn't open /io.stat file");
        return -1;
    }
    // Read the io.stat file
    long long total_IO = 0, total_rbytes = 0, total_wbytes = 0;
    char line[320];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long rbytes = 0, wbytes = 0, rios = 0, wios = 0, ios = 0;
        sscanf(line, "%*d:%*d rbytes=%llu wbytes=%llu rios=%llu wios=%llu dbytes=%*d dios=%*d",
            &rbytes, &wbytes, &rios, &wios);
        total_IO += rios + wios;
        total_rbytes += rbytes;
        total_wbytes += wbytes;
    }
    fclose(fp);
    cg_stats->io_op = total_IO;
    cg_stats->r_bytes = total_rbytes;
    cg_stats->w_bytes = total_wbytes;

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