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
#include <dirent.h>
#include "energy.h"
#include "process_stats.h"
#include "perf_events.h"
#include "container_stats.h"
#include "logging.h"
#include "benchmarking.h"
// #include "read_nvidia_gpu.h"

#define MAX_CPUS sysconf(_SC_NPROCESSORS_CONF)
#define CLK_TCK sysconf(_SC_CLK_TCK)
#define interval 1 // measurements taken in intervals (in seconds) for system-wide, -m and -c

static void print_pinfo(struct proc_stats *p_info);
static void print_system_stats(struct system_stats *system_info);
static void print_container_info(struct container_stats *container);
static void print_cgroup_stats(struct cgroup_stats *cg);
static void print_help();


int main(int argc, char *argv[]) {
    pid_t pid;
    int status, ret, fd;
    struct rusage usage;
    long long energy_before_pkg = 0; // microjoules
    long long energy_before_dram = 0; // microjoules
    long long energy_after_pkg = 0; // microjoules
    long long energy_after_dram = 0; // microjoules
    long long total_energy_used = 0; // microjoules
    struct system_stats system_stats = {0};
    int fds_cpu[MAX_CPUS];
    long long cpu_cycles = 0;
    int logging_enabled = 0; // Flag to indicate if logging is enabled
    char logging_buffer[4096] = "";
    FILE *logfile;
    
    init_rapl();

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
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            print_system_stats(&system_stats);
            printf("Interval(%d): total energy (microjoules): %lld, CPU-cycles: %lld\n", 
                interval, total_energy_used, system_stats.cycles);
            if(logging_enabled == 1) {
                system_stats_to_buffer(&system_stats, total_energy_used, logging_buffer);
                writeToFile(logfile, logging_buffer);
            }
        }
        
        return 0;
    }

    // -e (execute a given command, e.g. -e java myprogram)
    else if (strcmp(argv[1], "-e") == 0)
    {
        // Use cgroup to measure the process and subprocesses
        init_benchmarking();
        struct timeval start, end;
        double elapsedTime = 0;
        struct cgroup_stats cg_stats = {0};
        char first_part[64] = "cgexec -g cpu,memory,io:/benchmarking";
        char second_part[16] = " --sticky ";
        char command[512] = "";
        snprintf(command, sizeof(command), "%s_%d%s", first_part, cgroup_id, second_part);

        // Append command line arguments to command
        for (int i = 2; i < argc; i++) {
            strcat(command, argv[i]);
            strcat(command, " ");
        }

        // Create cgroup
        reset_cgroup();

        ret = read_systemwide_stats(&system_stats);
        cpu_cycles = 0;
        for (int i = 0; i < MAX_CPUS; i++) {
            fds_cpu[i] = setUpProcCycles_cpu(i);
        }
        gettimeofday(&start, NULL);
        energy_before_pkg = read_energy(0);
        energy_before_dram = read_energy(3);

        // Run the command
        system(command);

        // Measurements
        energy_after_pkg = read_energy(0);
        energy_after_dram = read_energy(3);
        gettimeofday(&end, NULL);
        for (int i = 0; i < MAX_CPUS; i++) {   
            cpu_cycles += readInterval(fds_cpu[i]);
            closeEvent(fds_cpu[i]);
        }
        ret = read_systemwide_stats(&system_stats);
        read_cgroup_stats(&cg_stats);
        system_stats.cycles = cpu_cycles;
        total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                            + check_overflow(energy_before_pkg, energy_after_pkg);
        elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * 1e-6;
        cg_stats.estimated_energy = estimate_energy_cycles(system_stats.cycles, cg_stats.cycles, total_energy_used, elapsedTime);
        print_cgroup_stats(&cg_stats);
        printf("Total energy in microjoules: %lld\n", total_energy_used);
        printf("Elapsed time: %f\n", elapsedTime);
        if (logging_enabled == 1) {
            system_interval_to_buffer(&system_stats, total_energy_used, logging_buffer);
            cgroup_stats_to_buffer(&cg_stats, elapsedTime, logging_buffer);
            writeToFile(logfile, logging_buffer);
            fclose(logfile);
        }

        close_cgroup();
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
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            // Update containers
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
        // - how much of idle power can be used on processes?
        // - coefficient to make up for measurements?

        // Save to file
        FILE *fp = fopen("config_idle.txt", "w");
        fprintf(fp, "%lld\n%lld\n", total_energy, min_value); // Write total_energy to the file
        printf("New idle energy consumption (microjoules per second): %lld\n", total_energy);
        fclose(fp);
        
        return 0;
    }

    // -b benchmarking
    /* benchmarking directory
        - language directories
            - algorithm directories
                - source code + run.txt
    */
    else if (strcmp(argv[1], "-b") == 0) 
    {
        if (argc=3) {
            init_benchmarking();
            // Read path to benchmark directory
            char* directory_path = argv[2];
            // Enter benchmark directory (languages)
            DIR *bench_dir = opendir(directory_path);
            struct dirent *lang_folder;
            if (bench_dir == NULL) {
                perror("Unable to open benchmark directory");
            }

            // Iterate through each directory (language)         
            while ((lang_folder = readdir(bench_dir)) != NULL) {
                // Exclude current and parent directory entries
                if (strcmp(lang_folder->d_name, ".") == 0 || strcmp(lang_folder->d_name, "..") == 0) {
                    continue;
                }
                // Construct the language directory path
                char lang_dir_path[1024];
                snprintf(lang_dir_path, sizeof(lang_dir_path), "%s/%s", directory_path, lang_folder->d_name);
                printf("Opening lang: %s\n", lang_dir_path); // TEST
                // Open the language directory
                DIR *lang_dir = opendir(lang_dir_path);
                if (lang_dir == NULL) {
                    perror("Unable to open language directory");
                    continue;
                }
                struct dirent *alg_entry;
                // Iterate through each directory (algorithm) inside the language directory
                while ((alg_entry = readdir(lang_dir)) != NULL) {
                    // Exclude current and parent directory entries
                    if (strcmp(alg_entry->d_name, ".") == 0 || strcmp(alg_entry->d_name, "..") == 0) {
                        continue;
                    }
                    // Construct the algorithm directory path
                    char alg_dir_path[1300];
                    snprintf(alg_dir_path, sizeof(alg_dir_path), "%s/%s", lang_dir_path, alg_entry->d_name);
                    printf("In working directory: %s\n", alg_dir_path); // TEST
                    // Change working directory
                    chdir(alg_dir_path);

                    // Reset cgroup statistics and system-wide measurements
                    char first_part[64] = "cgexec -g cpu,memory,io:/benchmarking";
                    char second_part[16] = " --sticky ";
                    char command[512] = "";
                    snprintf(command, sizeof(command), "%s_%d%s", first_part, cgroup_id, second_part);
                    char line[256];
                    FILE *fp = fopen("run.txt", "r");
                    if (fp == NULL) {
                        printf("no run.txt");
                        return -1;
                    }
                    if (fgets(line, sizeof(line), fp)) {
                        // Remove newline character from the end
                        line[strcspn(line, "\n")] = '\0';
                    }
                    fclose(fp);
                    // Concat run.txt
                    strcat(command, line);

                    // Measurements
                    struct timeval start, end;
                    double elapsedTime = 0;
                    struct cgroup_stats cg_stats;
                    reset_cgroup();

                    ret = read_systemwide_stats(&system_stats);
                    cpu_cycles = 0;
                    for (int i = 0; i < MAX_CPUS; i++) {
                        fds_cpu[i] = setUpProcCycles_cpu(i);
                    }
                    gettimeofday(&start, NULL);
                    energy_before_pkg = read_energy(0);
                    energy_before_dram = read_energy(3);

                    // Run the command
                    system(command);

                    // Measurements
                    energy_after_pkg = read_energy(0);
                    energy_after_dram = read_energy(3);
                    gettimeofday(&end, NULL);
                    for (int i = 0; i < MAX_CPUS; i++) {   
                        cpu_cycles += readInterval(fds_cpu[i]);
                        closeEvent(fds_cpu[i]);
                    }
                    ret = read_systemwide_stats(&system_stats);
                    read_cgroup_stats(&cg_stats);
                    system_stats.cycles = cpu_cycles;
                    total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                        + check_overflow(energy_before_pkg, energy_after_pkg);
                    elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * 1e-6;
                    cg_stats.estimated_energy = estimate_energy_cycles(system_stats.cycles, cg_stats.cycles, total_energy_used, elapsedTime);
                    print_cgroup_stats(&cg_stats);
                    printf("Total energy in microjoules: %lld\n", total_energy_used);
                    printf("Elapsed time: %f\n", elapsedTime);
                    // write result to log file cg_stats, energy, estimated energy, elapsed time?
                    logfile = initBenchLogFile(alg_entry->d_name, lang_folder->d_name);
                    system_interval_to_buffer(&system_stats, total_energy_used, logging_buffer);
                    cgroup_stats_to_buffer(&cg_stats, elapsedTime, logging_buffer);
                    writeToFile(logfile, logging_buffer);
                    fclose(logfile);
                    // Clear cache for consistent and fair I/O statistics
                    system("echo 3 > /proc/sys/vm/drop_caches");
                    // small break in between to avoid system cleanup activities?
                    sleep(3);
                }
                closedir(lang_dir);
            }
            closedir(bench_dir);
            close_cgroup();
        } else {
            printf("No directory path provided. \n");
        }

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
    printf("Resident set size change in bytes: %lld\n", container->memory_interval);
    printf("IO-operations: %ld\n", container->io_op_interval);
    printf("Number of CPU cycles: %llu\n", container->cycles_interval);
    printf("Estimated energy in microjoules: %lld\n", container->energy_interval_est);
}

static void print_cgroup_stats(struct cgroup_stats *cg) {
    printf("----------------------------------\n");
    printf("CPU-Time in microseconds: %llu\n", cg->cputime);
    printf("Max RSS in bytes: %lld\n", cg->maxRSS);
    printf("IO-operations: %lu; r_bytes: %llu, w_bytes: %llu\n", cg->io_op, cg->r_bytes, cg->w_bytes);
    printf("Number of CPU cycles: %llu\n", cg->cycles);
    printf("Estimated energy in microjoules: %lld\n", cg->estimated_energy);
}

static void print_help() {
    printf("Possible arguments: \n"
        " -l (logging in combination with others (except -b), has to be first) \n"
        " -e (execute a given command, e.g. -e java myprogram) \n"
        " -m (monitor given processes given by their id, e.g. -m 1 2 3) \n"
        " -c (monitor running docker containers) \n"
        " -i (calibration, execute on idle system for idle power) \n"
        " -b (benchmarking, path to directory with programs and run files) \n"
        " Running with no arguments or only -l will monitor all active processes.\n");
}
