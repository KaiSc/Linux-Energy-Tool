#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string.h>
#include "energy.h"
#include "process_stats.h"
#include "perf_events.h"
#include "container_stats.h"
// #include "read_nvidia_gpu.h"

#define CLK_TCK sysconf(_SC_CLK_TCK)
#define MAX_CPUS sysconf(_SC_NPROCESSORS_CONF)
#define interval 1 // measurements taken in intervals (in seconds) for systemwide and -m


static void print_pinfo(struct proc_info *p_info) {
    printf("----------------------------------\n");
    printf("Process: %d, statistics from last interval:\n", p_info->pid);
    printf("CPU-Time in seconds: %.2f \n", (double) p_info->cputime_interval / CLK_TCK);
    printf("Resident set size change in kB: %ld \n", p_info->rss_interval);
    printf("Number of IO operations: %ld \n", p_info->io_op_interval);
    printf("Number of CPU cycles: %lld \n", p_info->cycles_interval);
    printf("Estimated energy in microjoules: %.0f \n", p_info->energy_interval_est);
    printf("----------------------------------\n");
}

static void print_system_stats(struct system_stats *system_info) {
    printf("----------------------------------\n");
    printf("System statistics from last interval:\n");
    printf("CPU-Time in seconds: %.2f \n", (double) system_info->cputime_interval / CLK_TCK);
    printf("Resident set size in kB: %ld \n", system_info->rss_interval);
    printf("Number of I/O operations: %ld \n", system_info->io_op_interval);
    printf("Number of CPU cycles %lld\n", system_info->cycles);
    printf("----------------------------------\n");
}

static void print_help() {
    printf("Possible arguments: \n"
        " -e (execute a given command, e.g. -e java myprogram) \n"
        " -m (monitor given processes given by their id, e.g. -m 1 2 3) \n"
        " -c (monitor running docker containers) \n"
        " -i (calibrate estimation weights, saves in config file for future use) \n"
        " Running with no arguments will monitor all active processes.\n");
}

// TODO read energy with perf events?
int main(int argc, char *argv[]) {
    pid_t pid;
    int status, ret, fd;
    struct rusage usage;
    uint64_t energy_before_pkg = 0;
    uint64_t energy_before_dram = 0;
    uint64_t energy_after_pkg = 0;
    uint64_t energy_after_dram = 0;
    uint64_t total_energy_used = 0;
    struct system_stats system_stats;
    int fds_cpu[MAX_CPUS];
    long long cpu_cycles = 0;
    
    init_rapl();

    // No arguments provided, system-wide monitoring
    if (argc < 2) 
    {
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        while (1)
        {
            cpu_cycles = 0;
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);

            sleep(interval);

            for (int i = 0; i < MAX_CPUS; i++) {   
                cpu_cycles += readInterval(fds_cpu[i]);
            }
            system_stats.cycles = cpu_cycles;
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            ret = read_systemwide_stats(&system_stats);
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            print_system_stats(&system_stats);
            printf("Total microjoules used: %"PRIu64 "\n", total_energy_used);
        }
        
        return 0;
    }

    // -e (execute a given command, e.g. -e java myprogram)
    else if (strcmp(argv[1], "-e") == 0)
    {
        // Fork to start child process
        argv[argc] = NULL; // Add NULL terminator to argument for execvp
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        read_systemwide_stats(&system_stats);
        energy_before_pkg = read_energy(0);
        energy_before_dram = read_energy(3);

        pid = fork();
        if (pid == 0) {
            // Child process executes the command
            execvp(argv[2], &argv[2]); // replaces the child process with this one
        } else if (pid > 0) {
            long long proc_cycles = 0;
            int fd = setUpProcCycles(pid);
            // parent process waits for child termination 
            // TODO instead use some signal that keeps child process in zombie state, ptrace?

            /*  // higher energy overhead but same statistics as running processes
            struct proc_info p;
            p.pid = pid;
            ret = 0;
            while (ret == 0)
            {
                read_process_stats(&p); // need to read in loop else rss will be 0
                usleep(10000); //sleep 10ms and check if terminated
                ret = check_zombie_state(pid);
            }
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            total_energy_used = check_overflow(energy_before_pkg, energy_after_pkg)
                                    + check_overflow(energy_before_dram, energy_after_dram);
            print_pinfo(&p);
            printf("Systemwide measured energy in microjoules: %"PRIu64 "\n", total_energy_used);
            /* */

            /* // more accurate energy but different statistics */
            // TODO overflow
            int ret = wait4(pid, &status, 0, &usage);
            if (ret != -1) {
                energy_after_pkg = read_energy(0);
                energy_after_dram = read_energy(3);
                total_energy_used = check_overflow(energy_before_pkg, energy_after_pkg)
                                    + check_overflow(energy_before_dram, energy_after_dram);
                proc_cycles += readInterval(fd);
                closeEvent(fd);
                read_systemwide_stats(&system_stats);
                for (int i = 0; i < MAX_CPUS; i++) {   
                    cpu_cycles += readInterval(fds_cpu[i]);
                    closeEvent(fds_cpu[i]);
                }
                system_stats.cycles = cpu_cycles;
                double estimated_energy = estimate_energy_cycles(cpu_cycles, proc_cycles, total_energy_used);

                printf("User CPU time: %ld seconds, %ld microseconds\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
                printf("System CPU time: %ld seconds, %ld microseconds\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
                printf("Maximum used rss: %ld \n", usage.ru_maxrss);
                printf("Input/Output operations: %ld \n", usage.ru_inblock + usage.ru_oublock);
                printf("Process CPU cycles: %lld \n", proc_cycles);
                printf("System-wide CPU cycles: %lld \n", cpu_cycles);
                printf("Estimated energy: %.0f \n", estimated_energy);
                printf("System-wide measured energy in microjoules: %"PRIu64 "\n", total_energy_used);
            }
            /* */

        } else {
            // fork failed
            printf("Fork failed\n");
            exit(1);
        }

        return 0;
    }

    // -m (monitor given processes given by their id, e.g. -m 1 2 3)
    else if (strcmp(argv[1], "-m") == 0)
    {
        int num_processes = argc - 2; // number of processes to monitor
        struct proc_info *processes = malloc(num_processes * sizeof(struct proc_info)); // allocate memory
        // Create proc_info for each process id
        for (int i = 0; i < num_processes; i++)
        { 
            pid = (pid_t) atoi(argv[i+2]);
            int proc_fd = setUpProcCycles(pid);
            processes[i].pid = pid;
            processes[i].cputime = 0;
            processes[i].rss = 0;
            processes[i].io_op = 0;
            processes[i].cputime_interval = 0;
            processes[i].rss_interval = 0;
            processes[i].io_op_interval = 0;
            processes[i].fd = proc_fd;
            processes[i].energy_interval_est = 0;
            ret = read_process_stats(&processes[i]);
        }
        ret = read_systemwide_stats(&system_stats);
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++)
        {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        while (num_processes > 0) 
        {
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);
            cpu_cycles = 0;

            sleep(interval);

            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            read_systemwide_stats(&system_stats);
            total_energy_used = check_overflow(energy_before_pkg, energy_after_pkg)
                                + check_overflow(energy_before_dram, energy_after_dram);
            
            for (int i = 0; i < MAX_CPUS; i++)
            {   
                cpu_cycles += readInterval(fds_cpu[i]);
            }
            system_stats.cycles = cpu_cycles;
            // Update/remove ended processes
            for (int i = 0; i < num_processes; i++)
            {
                ret = read_process_stats(&processes[i]);
                processes[i].cycles_interval = readInterval(processes[i].fd);
                // TODO process ended in interval, perf event recorded until end, final output?
                if (ret == -1) 
                {
                    // process has terminated, remove it from the array
                    closeEvent(processes[i].fd);
                    if (num_processes==1) { // all processes terminated
                        return 0;
                    }
                    for (int j = i; j < num_processes - 1; j++) {
                        processes[j] = processes[j + 1];
                    }
                    num_processes--;
                    i--; // correct current index
                }

                // estimage energy
                processes[i].energy_interval_est = estimate_energy_cycles(system_stats.cycles,
                                                processes[i].cycles_interval, total_energy_used);
                //processes[i].energy_interval_est = estimate_energy_cputime(system_stats.cputime_interval,
                //                                processes[i].cputime_interval, total_energy_used);
                print_pinfo(&processes[i]);
            }

            printf("Total used energy in time interval(%d) in microjoules: %"PRIu64 "\n", 
                interval, total_energy_used);
        }
        free(processes);
    }

    // -c (monitor running docker containers) 
    else if (strcmp(argv[1], "-c") == 0)
    {
        // TODO ////
        get_containers();
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
            // TODO setUpProcCycles_cgroup(cgroup_fd, i)
        }
        while(1) {
            cpu_cycles = 0;
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);
            // TODO update container list
            sleep(interval);

            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            ret = read_systemwide_stats(&system_stats);
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            for (int i = 0; i < MAX_CPUS; i++) {   
                cpu_cycles += readInterval(fds_cpu[i]);
                // TODO cgroup cycles
            }
            system_stats.cycles = cpu_cycles;
        }

        // TODO ////
    }

    // -i help information
    else if (strcmp(argv[1], "-i") == 0) 
    {
        // TODO calibration
        // measure and save idle power consumption
        // measure all cpus maxed out?
    }

    // -h help information
    else if (strcmp(argv[1], "-h") == 0) 
    {
        print_help();
    }

    else 
    {
        printf("Could not match given arguments. Run again with -h for help \n");
    }
}