#include <stdio.h>
#include <sys/resource.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include "energy.h"

// CPU-Time, I/O, Memory used

struct proc_info { 
    pid_t pid;
    unsigned long cputime; // in clock ticks, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kilobytes
    long io_op; 
    uint64_t energy_interval_est; // in microjoules
};


int read_process_stats(struct proc_info *p_info) {
    char stat_path[64];
    char io_path[64];
    char status_path[64];

    // save previous values for energy estimation
    unsigned long delta_cputime = p_info->cputime; 
    long delta_rss = p_info->rss;
    long delta_io_op = p_info->io_op;

    // read /proc/pid/stat file for cpu time
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", p_info->pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) {
        perror("Couldn't open /proc/pid/stat file");
        return -1;
    }
    unsigned long current_utime; // user cpu time
    unsigned long current_stime; // system cpu time
    char state;
    // parenthesis skip for name field, utime14, stime15, 
    int ret = fscanf(fp, "%*d (%*[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu ",
            &state, &current_utime, &current_stime);
    fclose(fp);
    p_info->cputime = current_utime + current_stime;

    // read proc/pid/io file for io operations
    snprintf(io_path, sizeof(io_path), "/proc/%d/io", p_info->pid);
    fp = fopen(io_path, "r");
    if (!fp) {
        perror("Couldn't open /proc/pid/io file");
        return -1;
    }
    unsigned long long io_read, io_write;
    unsigned long syscr, syscw;
    fscanf(fp, "rchar: %llu\n", &io_read);
    fscanf(fp, "wchar: %llu\n", &io_write);
    fscanf(fp, "syscr: %lu\n", &syscr);
    fscanf(fp, "syscw: %lu\n", &syscw);
    unsigned long io_operations = syscr + syscw;
    p_info->io_op = io_operations;
    fclose(fp);

    // read proc/pid/status file for resident set size
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", p_info->pid);
    fp = fopen(status_path, "r");
    if (!fp) {
        perror("Couldn't open /proc/pid/status file");
        return -1;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) { // read line by line
        if (strncmp(line, "VmRSS:", 6) == 0) { // break when right line found
            sscanf(line + 6, "%lu", &p_info->rss);
            break;
        }
    }
    fclose(fp);

    // if just created, skip estimation
    if (p_info->rss == 0) {
        return 0;
    }
    // calculate difference for time window
    delta_cputime = p_info->cputime - delta_cputime;
    delta_rss = p_info->rss - delta_rss;
    delta_io_op = p_info->io_op - delta_io_op;
    // estimate energy
    p_info->energy_interval_est = estimate_energy(delta_cputime, delta_rss, delta_io_op);

    return 0;
}


int read_systemwide_stats(struct proc_info *p_info) {
    // read /proc/stat file for systemwide cpu time
    char line[256];
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Couldn't open /proc/stat file");
        return -1;
    }
    // add up all besides
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    unsigned long long total_cpu_time = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu ", 4) != 0) { // should be first line
            continue;
        }
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", 
            &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);
        total_cpu_time = user + nice + system + iowait + irq + softirq + steal + guest + guest_nice;
        break;
    }
    fclose(fp);
    p_info->cputime = total_cpu_time;

    // read /proc/meminfo file for systemwide memory usage
    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("Couldn't open /proc/meminfo file");
        return -1;
    }

    unsigned long mem_total, mem_free;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &mem_total);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %ld kB", &mem_free);
        }
    }
    fclose(fp);
    p_info->rss = mem_total - mem_free;

    // read /proc/diskstats file for systemwide io operations
    fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("Couldn't open /proc/diskstats file");
        return -1;
    }

    unsigned long read_op, write_op;
    char device_name[32];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long read, write;
        sscanf(line, "%*u %*u %s %lu %*u %*u %*u %lu", 
            device_name, &read, &write);
        // TODO skip loop device names virtual not actual io?
        if (strcmp(device_name, "loop") == 0) {
            continue;
        }

        read_op += read;
        write_op += write;
    }
    fclose(fp);
    p_info->io_op = read_op + write_op;

    return 0;
}

int check_zombie_state(pid_t pid) {
    char stat_path[64];
    char state;
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", (int) pid);
    FILE *fp = fopen(stat_path, "r");
    if (!fp) {
        perror("Couldn't open /proc/pid/stat file");
        return -1;
    }
    // parenthesis skip for name field
    int ret = fscanf(fp, "%*d (%*[^)]) %c", &state);
    fclose(fp);
    if (state == 'Z' || state == 'X') { // process terminated or zombie state
        return 1;
    }
    return 0;
}
