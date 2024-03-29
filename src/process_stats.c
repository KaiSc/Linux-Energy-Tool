#include <stdio.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string.h>
#include "energy.h"

// CPU-Time, I/O, Memory used

struct proc_stats { 
    pid_t pid;
    unsigned long cputime; // in clock ticks, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kilobytes
    long io_op; 
    unsigned long cputime_interval;
    long rss_interval;
    long io_op_interval;
    long long cycles_interval;
    int fd;
    long long energy_interval_est; // in microjoules
};

struct system_stats {
    unsigned long cputime; // in clock ticks, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kilobytes
    long io_op;
    unsigned long cputime_interval;
    long rss_interval;
    long io_op_interval;
    long long cycles;
};


int read_process_stats(struct proc_stats *p_info) {
    char path[64];

    // Save previous values for energy estimation
    unsigned long delta_cputime = p_info->cputime; 
    long delta_rss = p_info->rss;
    long delta_io_op = p_info->io_op;

    // Read /proc/pid/stat file for cpu time
    snprintf(path, sizeof(path), "/proc/%d/stat", p_info->pid);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Couldn't open /proc/pid/stat file");
        return -1;
    }
    unsigned long current_utime; // user cpu time in jiffies
    unsigned long current_stime; // system cpu time in jiffies
    char state;
    // Parenthesis skip for name field, utime14, stime15, 
    int ret = fscanf(fp, "%*d (%*[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu ",
            &state, &current_utime, &current_stime);
    fclose(fp);
    p_info->cputime = current_utime + current_stime;

    // Read proc/pid/io file for I/O operations
    snprintf(path, sizeof(path), "/proc/%d/io", p_info->pid);
    fp = fopen(path, "r");
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

    // Read proc/pid/status file for resident set size
    snprintf(path, sizeof(path), "/proc/%d/status", p_info->pid);
    fp = fopen(path, "r");
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

    // If just created, skip
    if (p_info->rss == 0) {
        return 0;
    }
    // Calculate difference for time window
    p_info->cputime_interval = p_info->cputime - delta_cputime;
    p_info->rss_interval = p_info->rss - delta_rss;
    p_info->io_op_interval = p_info->io_op - delta_io_op;

    return 0;
}

int read_systemwide_stats(struct system_stats *sys_stats) {
    // Save previous values for energy estimation
    unsigned long delta_cputime = sys_stats->cputime; 
    long delta_rss = sys_stats->rss;
    long delta_io_op = sys_stats->io_op;

    // Read /proc/stat file for systemwide cpu time
    char line[256];
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("Couldn't open /proc/stat file");
        return -1;
    }
    // Add up all besides idle
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice; // in jiffies
    unsigned long long total_cpu_time = 0; // in jiffies

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
    sys_stats->cputime = total_cpu_time;

    // Read /proc/meminfo file for systemwide memory usage
    fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("Couldn't open /proc/meminfo file");
        return -1;
    }

    unsigned long mem_total, mem_free; // in kB
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line, "MemTotal: %ld kB", &mem_total);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line, "MemFree: %ld kB", &mem_free);
        }
    }
    fclose(fp);
    sys_stats->rss = mem_total - mem_free;

    // Read /proc/diskstats file for systemwide I/O operations
    fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("Couldn't open /proc/diskstats file");
        return -1;
    }

    unsigned long read_op = 0, write_op = 0;
    char device_name[32];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long read = 0, write = 0;
        sscanf(line, "%*u %*u %s %lu %*u %*u %*u %lu", 
            device_name, &read, &write);
        // Skip loop device names virtual not actual io?
        if (strncmp(device_name, "loop", 4) == 0){
                    continue;
        }

        read_op += read;
        write_op += write;
    }
    fclose(fp);
    sys_stats->io_op = read_op + write_op;

    // Compute interval statistics
    if (delta_cputime==0) {return 0;}
    sys_stats->cputime_interval = sys_stats->cputime - delta_cputime;
    sys_stats->rss_interval = sys_stats->rss - delta_rss;
    sys_stats->io_op_interval = sys_stats->io_op - delta_io_op;

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
    // Parenthesis skip for name field
    int ret = fscanf(fp, "%*d (%*[^)]) %c", &state);
    fclose(fp);
    if (state == 'Z' || state == 'X') { // process terminated or zombie state
        return 1;
    }
    return 0;
}
