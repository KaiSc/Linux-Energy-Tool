#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <float.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string.h>
#include "energy.h"
#include "process_stats.h"
#include "perf_events.h"
#include "container_stats.h"
#include "logging.h"
#include "read_nvidia_gpu.h"

#define MAX_CPUS sysconf(_SC_NPROCESSORS_CONF)
#define CLK_TCK sysconf(_SC_CLK_TCK)
#define interval 1 // measurements taken in intervals (in seconds) for system-wide, -m and -c

static void print_pinfo(struct proc_stats *p_info);
static void print_system_stats(struct system_stats *system_info);
static void print_container_info(struct container_stats *container);
static void print_help();


int main(int argc, char *argv[]) {
    pid_t pid;
    int status, ret, fd;
    struct rusage usage;
    long long energy_before_pkg = 0;
    long long energy_before_dram = 0;
    long long energy_after_pkg = 0;
    long long energy_after_dram = 0;
    long long total_energy_used = 0;
    struct system_stats system_stats;
    int fds_cpu[MAX_CPUS];
    long long cpu_cycles = 0;
    int logging_enabled = 0; // Flag to indicate if logging is enabled
    char logging_buffer[4096] = "";
    FILE *logfile;
    
    init_rapl();
    init_gpu();

    // Check if the first argument is '-l' and set logging flag
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        logging_enabled = 1;
        // Remove '-l' from the argument list
        for (int i = 1; i < argc - 1; i++) {
            argv[i] = argv[i + 1];
        }
        argc--;
        logfile = initLogFile();
    }

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
            ret = read_systemwide_stats(&system_stats);

            sleep(interval);

            for (int i = 0; i < MAX_CPUS; i++) {   
                cpu_cycles += readInterval(fds_cpu[i]);
            }
            system_stats.cycles = cpu_cycles;
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            ret = read_systemwide_stats(&system_stats);
            get_gpu_stats();
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            print_system_stats(&system_stats);
            printf("Interval(%d): total energy (microjoules): %lld, CPU-cycles: %lld\n", 
                interval, total_energy_used, system_stats.cycles);
            if(logging_enabled == 1) {
                system_stats_to_buffer(&system_stats, total_energy_used, logging_buffer);
                gpu_stats_to_buffer(logging_buffer);
                writeToFile(logfile, logging_buffer);
            }
        }
        
        return 0;
    }

    // -e (execute a given command, e.g. -e java myprogram)
    else if (strcmp(argv[1], "-e") == 0)
    {
        // Fork to start child process
        argv[argc] = NULL; // Add NULL terminator to argument for execvp
        struct timeval start, end;
        double elapsedTime;
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        double time = 0;
        read_systemwide_stats(&system_stats);
        gettimeofday(&start, NULL);
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
            /*  // higher energy overhead but same statistics as running processes
            struct proc_stats p;
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
            printf("Systemwide measured energy in microjoules: %lld" "\n", total_energy_used);
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
                gettimeofday(&end, NULL);
                closeEvent(fd);
                read_systemwide_stats(&system_stats);
                for (int i = 0; i < MAX_CPUS; i++) {   
                    cpu_cycles += readInterval(fds_cpu[i]);
                    closeEvent(fds_cpu[i]);
                }
                system_stats.cycles = cpu_cycles;
                elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * 1e-6;
                long long estimated_energy = estimate_energy_cycles(cpu_cycles, proc_cycles,
                                                                    total_energy_used, elapsedTime);
                double total_proc_cpu_time = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + 
                        (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1e-6;
                long total_io = usage.ru_inblock + usage.ru_oublock;
                //printf("Execution time: %.6f\n", elapsedTime);
                printf("Process CPU Time: %.6f\n", total_proc_cpu_time);
                printf("Maximum used rss: %ld \n", usage.ru_maxrss);
                printf("Input/Output operations: %ld \n", total_io);
                printf("Process CPU cycles: %lld \n", proc_cycles);
                printf("System-wide CPU cycles: %lld \n", cpu_cycles);
                printf("Estimated energy: %lld \n", estimated_energy);
                printf("System-wide measured energy in microjoules: %lld\n", total_energy_used);
                if (logging_enabled == 1) {
                    system_stats_to_buffer(&system_stats, total_energy_used, logging_buffer);
                    e_stats_to_buffer(total_proc_cpu_time, usage.ru_maxrss, total_io,
                                proc_cycles, estimated_energy, logging_buffer);
                    writeToFile(logfile, logging_buffer);
                }
            }
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
        int num_processes = argc - 2;
        struct proc_stats *processes = malloc(num_processes * sizeof(struct proc_stats)); // allocate memory
        // Create proc_stats for each process id
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
            get_gpu_stats();
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
                if (ret == -1) 
                {
                    // Process has terminated, remove it from the array
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

                // Estimate energy
                processes[i].energy_interval_est = estimate_energy_cycles(system_stats.cycles,
                                processes[i].cycles_interval, total_energy_used, interval);
                print_pinfo(&processes[i]);
            }
            printf("Interval(%d): total energy (microjoules): %lld, CPU-cycles: %lld\n", 
                interval, total_energy_used, system_stats.cycles);
            if (logging_enabled == 1) {
                // Logging
                system_stats_to_buffer(&system_stats, total_energy_used, logging_buffer);
                gpu_stats_to_buffer(logging_buffer);
                for (int i = 0; i < num_processes; i++)
                {
                    process_stats_to_buffer(&processes[i], logging_buffer);
                }
                writeToFile(logfile, logging_buffer);
            }
        }
        free(processes);
    }

    // -c (monitor running docker containers) 
    else if (strcmp(argv[1], "-c") == 0)
    {
        init_docker_container();
        // Set up system-wide cycles
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        get_docker_containers();
        while(1) {
            cpu_cycles = 0;
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);
            sleep(interval);
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            ret = read_systemwide_stats(&system_stats);
            get_gpu_stats();
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            // Update containers
            // TODO estimate energy in updating
            update_docker_containers();
            for (int i = 0; i < MAX_CPUS; i++) {   
                cpu_cycles += readInterval(fds_cpu[i]);
            }
            system_stats.cycles = cpu_cycles;
            // Estimate energy
            for (int i = 0; i < num_containers; i++)
            {
                containers[i].energy_interval_est = estimate_energy_cycles(cpu_cycles,
                    containers[i].cycles_interval, total_energy_used, interval);
                print_container_info(&containers[i]);
            }
            printf("Interval(%d): total energy (microjoules): %lld, CPU-cycles: %lld\n", 
                interval, total_energy_used, system_stats.cycles);
            if (logging_enabled == 1) {
                // Logging
                system_stats_to_buffer(&system_stats, total_energy_used, logging_buffer);
                gpu_stats_to_buffer(logging_buffer);
                for (int i = 0; i < num_containers; i++)
                {
                    container_stats_to_buffer(&containers[i], logging_buffer);
                }
                writeToFile(logfile, logging_buffer);
            }
        }
    }

    // -i (calibration, execute on idle system for idle energy per second)
    else if (strcmp(argv[1], "-i") == 0) 
    {
        int measurements = 181;
        long long measured_values[measurements];

        for (int i = 0; i < measurements; i++)
        {
            measured_values[i] = read_energy(0) + read_energy(3);
            sleep(1);
        }

        long long total_energy = 0;
        long long min_value = __LONG_LONG_MAX__;
        long long cur_interval = 0;

        for (int i = 0; i < measurements-1; i++)
        {
            cur_interval = check_overflow(measured_values[i], measured_values[i+1]);
            if (min_value > cur_interval) {
                min_value = cur_interval;
            }
            total_energy += cur_interval;
        }

        total_energy = total_energy / (measurements-1);
        // TODO
        // - how much of idle power can be used on processes 
        // - coefficient to make up for measurements?

        // Save to file
        FILE *fp = fopen("config_idle.txt", "w");
        fprintf(fp, "%lld\n%lld\n", total_energy, min_value); // Write total_energy to the file
        printf("New idle energy consumption (microjoules per second): %lld\n", total_energy);
        fclose(fp);
        
        return 0;
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


static void print_pinfo(struct proc_stats *p_info) {
    printf("----------------------------------\n");
    printf("Process: %d, statistics from last interval:\n", p_info->pid);
    printf("CPU-Time in seconds: %.2f \n", (double) p_info->cputime_interval / CLK_TCK);
    printf("Resident set size change in kB: %ld \n", p_info->rss_interval);
    printf("Number of IO operations: %ld \n", p_info->io_op_interval);
    printf("Number of CPU cycles: %lld \n", p_info->cycles_interval);
    printf("Estimated energy in microjoules: %lld \n", p_info->energy_interval_est);
}

static void print_system_stats(struct system_stats *system_info) {
    printf("----------------------------------\n");
    printf("System statistics from last interval:\n");
    printf("CPU-Time in seconds: %.2f \n", (double) system_info->cputime_interval / CLK_TCK);
    printf("Resident set size in kB: %ld \n", system_info->rss_interval);
    printf("Number of I/O operations: %ld \n", system_info->io_op_interval);
    printf("Number of CPU cycles: %lld\n", system_info->cycles);
}

static void print_container_info(struct container_stats *container) {
    printf("----------------------------------\n");
    printf("Container: %s\n", container->id);
    printf("CPU-Time in microseconds: %lu\n", container->cputime_interval);
    printf("Resident set size change in Bytes: %lld\n", container->memory_interval);
    printf("IO-operations: %ld\n", container->io_op_interval);
    printf("Number of CPU cycles: %llu\n", container->cycles_interval);
    printf("Estimated energy in microjoules: %lld\n", container->energy_interval_est);

}

static void print_help() {
    printf("Possible arguments: \n"
        " -l (logging in combination with others, has to be first) \n"
        " -e (execute a given command, e.g. -e java myprogram) \n"
        " -m (monitor given processes given by their id, e.g. -m 1 2 3) \n"
        " -c (monitor running docker containers) \n"
        " -i (calibration, execute on idle system for idle power) \n"
        " Running with no arguments or only -l will monitor all active processes.\n");
}
