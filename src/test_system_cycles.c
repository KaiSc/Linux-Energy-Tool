#include "perf_events.h"
#include <stdio.h>
#include <dirent.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#define MAX_PROCESSES 1024

bool is_number(const char* str) {
    if (*str == '\0') {
        return false;
    }

    while (*str != '\0') {
        if (*str < '0' || *str > '9') {
            return false;
        }
        str++;
    }

    return true;
}

int main() {
    DIR* proc_dir = opendir("/proc");
    struct dirent* entry;
    int process_ids[MAX_PROCESSES];
    int num_processes = 0;

    if (proc_dir == NULL) {
        perror("opendir");
        return 1;
    }

    while ((entry = readdir(proc_dir)) != NULL && num_processes < MAX_PROCESSES) {
        if (entry->d_type == DT_DIR && is_number(entry->d_name)) {
            int pid = atoi(entry->d_name);
            process_ids[num_processes++] = pid;
        }
    }

    closedir(proc_dir);

    // Print the retrieved process IDs
    printf("Currently active processes %d:\n", num_processes);


    // Open perf events
    int fd_pkg;
    int fd_ram;
    int supported;
    supported = initEnergy();
    fd_pkg = openPkgEvent();
    if (supported == 0) {
        fd_ram = openRamEvent();
    }

    int max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    int fds_cpu[max_cpus];
    long long total_interval_cycles_cpu = 0;
    for (int i = 0; i < max_cpus; i++) {
        int ret = fds_cpu[i] = setUpProcCycles_cpu(i);
    }

    //
    int fds[MAX_PROCESSES];
    long long total_interval_cycles = 0;

    for (int i = 0; i < num_processes; i++) {
        pid_t pid = process_ids[i];
        fds[i] = setUpProcCycles(pid);
    }


    sleep(1);

    readEnergyInterval(fd_pkg, 0);
    if (supported==0) {
        readEnergyInterval(fd_ram, 1);
    }

    closeEvent(fd_pkg);
    closeEvent(fd_ram);

    // Open perf events and read the counters
    for (int i = 0; i < max_cpus; i++) {
        int fd = fds_cpu[i];
        long long cycles = readInterval(fd);
        total_interval_cycles_cpu += cycles;
    }

    //
    for (int i = 0; i < num_processes; i++) {
        int fd = fds[i];
        long long cycles = readInterval(fd);
        total_interval_cycles += cycles;
    }

    printf("Total measured cycles cpus: %lld\n", total_interval_cycles_cpu);
    printf("Total measured cycles: %lld\n", total_interval_cycles);

    // Close the perf events
    for (int i = 0; i < max_cpus; i++) {
        int fd = fds[i];
        closeEvent(fd);
    }


    for (int i = 0; i < num_processes; i++) {
        int fd = fds[i];
        closeEvent(fd);
    }

    return 0;
}
