#include <stdio.h>
#include <time.h>
#include <string.h>
#include "process_stats.h"
#include "container_stats.h"
#include "benchmarking.h"

FILE* initLogFile() {
    time_t rawtime;
    struct tm *timeinfo;
    char filename[100];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(filename, sizeof(filename), "logfile_%Y%m%d%H%M%S.txt", timeinfo);

    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        printf("Error creating file.\n");
    }

    return file;
}

FILE* initBenchLogFile(char* alg_name, char* lang_name) {
    time_t rawtime;
    struct tm *timeinfo;
    char filename[256];
    char timestamp[128];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S.txt", timeinfo);
    snprintf(filename, sizeof(filename), "%s_%s_%s", lang_name, alg_name, timestamp);

    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        printf("Error creating file.\n");
    }

    return file;
}

int writeToFile(FILE *fp, char* buffer) {
    if (fp == NULL) {
        printf("Invalid file pointer.\n");
        return -1;
    }
    // Write buffer to file
    fprintf(fp, "%s", buffer);
    // Flush the output buffer
    fflush(fp);
    // Clear buffer
    buffer[0] = '\0';

    return 0;
}

int system_stats_to_buffer(struct system_stats *s_stats, long long energy, char* buffer) {
    char toString[256];
    // energy_rapl_total, cputime_jiffies, ram_kB, io_op, cycles
    sprintf(toString, "%lld;%lu;%ld;%ld;%lld\n", energy, s_stats->cputime, s_stats->rss, 
            s_stats->io_op, s_stats->cycles);
    sprintf(buffer + strlen(buffer), "%s", toString);
    return 0;
}

int process_stats_to_buffer(struct proc_stats *p_stats, char* buffer) {
    char toString[256];
    // pid, cputime_jiffies, ram_kB, io_op, cycles, estimated energy
    sprintf(toString, "%d;%lu;%ld;%ld;%lld;%lld\n", p_stats->pid, p_stats->cputime,
            p_stats->rss, p_stats->io_op, p_stats->cycles_interval, p_stats->energy_interval_est);

    strcat(buffer, toString);
    return 0;
}

int container_stats_to_buffer(struct container_stats *c_stats, char* buffer) {
    char toString[370];
    // id, cputime_us, ram_bytes, io_op, cycles, estimated energy
    sprintf(toString, "%s;%llu;%lld;%lu;%llu;%lld\n", c_stats->id, c_stats->cputime,
            c_stats->memory, c_stats->io_op, c_stats->cycles_interval, c_stats->energy_interval_est);

    strcat(buffer, toString);
    return 0;
}

int e_stats_to_buffer(double cpu_time, long max_rss, long io, long long cycles, long long energy, char* buffer) {
    char toString[256];
    // cputime_s, ram_bytes, io_op, cycles, estimated_energy_uj
    sprintf(toString, "%.6f;%ld;%ld;%lld;%lld\n", cpu_time, max_rss, io, cycles, energy);

    strcat(buffer, toString);
    return 0;
}

int system_interval_to_buffer(struct system_stats *s_stats, long long energy, char* buffer) {
    char toString[256];
    // energy_total_rapl_uj, cputime_jiffies, ram_kB, io_op, cycles
    sprintf(toString, "%lld;%lu;%ld;%ld;%lld\n", energy, s_stats->cputime_interval, s_stats->rss_interval, 
            s_stats->io_op_interval, s_stats->cycles);
    strcat(buffer, toString);
    return 0;
}

int system_interval_gpu_to_buffer(struct system_stats *s_stats, long long energy, long long gpu_energy, char* buffer) {
    char toString[256];
    // energy_total_rapl_uj, gpu_energy, cputime_jiffies, ram_kB, io_op, cycles
    sprintf(toString, "%lld;%lld;%lu;%ld;%ld;%lld\n", energy, gpu_energy, s_stats->cputime_interval, s_stats->rss_interval, 
            s_stats->io_op_interval, s_stats->cycles);
    strcat(buffer, toString);
    return 0;
}

int cgroup_stats_to_buffer(struct cgroup_stats *c_stats, double time, char* buffer) {
    char toString[512];
    // time, cputime_us, max_ram_bytes, io_op, r_bytes, w_bytes cycles, estimated_energy_uj
    sprintf(toString, "%f;%llu;%lld;%lu;%llu;%llu;%llu;%lld\n", time, c_stats->cputime, c_stats->maxRSS,
            c_stats->io_op, c_stats->r_bytes, c_stats->w_bytes ,c_stats->cycles, c_stats->estimated_energy);

    strcat(buffer, toString);
    return 0;
}
