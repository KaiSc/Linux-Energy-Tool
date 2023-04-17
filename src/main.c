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
// #include "read_nvidia_gpu.h"


#define CLK_TCK sysconf(_SC_CLK_TCK)
#define interval 5 // measurements taken in intervals (in seconds) for systemwide and -m

/* arguments: -e cmd path;
              -m pid1 pid2 ...;
              -c (calibrate estimation); // TODO
              no arguments systemwide
*/

static void print_pinfo(struct proc_info *p_info) {
    printf("----------------------------------\n");
    printf("Process: %d \n", p_info->pid);
    printf("Current CPU-Time in seconds: %.2f \n", (double) p_info->cputime / CLK_TCK);
    printf("Current resident set size in kB: %ld \n", p_info->rss);
    printf("Current number of IO operations: %ld \n", p_info->io_op);
    printf("Estimated energy in microjoules: %"PRIu64 "\n", p_info->energy_interval_est);
    printf("----------------------------------\n");
}

static void print_help() {
    printf("Possible arguments: \n"
        " -e (execute a given command, e.g. -e java myprogram) \n"
        " -m (monitor given processes given by their id, e.g. -m 1 2 3) \n"
        " -c (calibrate estimation weights, saves in config file for future use) \n"
        " Running with no arguments will monitor all active processes.\n");
}


int main(int argc, char *argv[]) {
    pid_t pid;
    int status;
    struct rusage usage;
    uint64_t energy_before_pkg = 0;
    uint64_t energy_after_pkg = 0;
    uint64_t energy_before_dram = 0;
    uint64_t energy_after_dram = 0;
    uint64_t total_energy_used = 0;
    struct proc_info p_info;
    
    init_rapl();

    // no arguments provided, monitor everything
    if (argc < 2) 
    {
        // TODO ////
        // monitor all processes, systemwide TODO all individual processes
        p_info.pid = 0;
        while (1)
        {   
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);
            sleep(interval);
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            int ret = read_systemwide_stats(&p_info);
            total_energy_used = check_overflow(energy_before_dram, energy_after_dram) 
                                + check_overflow(energy_before_pkg, energy_after_pkg);
            print_pinfo(&p_info);
            printf("Total energy used: %"PRIu64 "\n", total_energy_used);
        }
        
        // TODO ////
        return 0;
    }
    // command line argument provided -e
    else if (strcmp(argv[1], "-e") == 0)
    {
        // fork to start child process
        argv[argc] = NULL; // Add NULL terminator to argument for execvp
        energy_before_pkg = read_energy(0);
        energy_before_dram = read_energy(3);
        pid = fork();
        if (pid == 0) {
            // child process executes the command (command must be in sys search path)
            execvp(argv[2], &argv[2]); // replaces the child process with this one
            exit(0);
        } else if (pid > 0) {
            // parent process waits for child termination 
            // TODO instead use some signal that keeps child process in zombie state, ptrace?

            /* */ // higher energy overhead but same statistics as running processes
            struct proc_info p;
            p.pid = pid;
            int ret = 0;
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

            /* // more accurate energy but different statistics
            int ret = wait4(pid, &status, 0, &usage);
            if (ret != -1) {
                energy_after_pkg = read_energy(0);
                energy_after_dram = read_energy(3);
                total_energy_used = check_overflow(energy_before_pkg, energy_after_pkg)
                                    + check_overflow(energy_before_dram, energy_after_dram);
                printf("User time: %ld seconds, %ld microseconds\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
                printf("System time: %ld seconds, %ld microseconds\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
                printf("Total CPU-Time: %ld.%ld \n", usage.ru_utime.tv_sec+usage.ru_stime.tv_sec, usage.ru_utime.tv_usec+usage.ru_stime.tv_usec);
                printf("Maximum used rss: %ld \n", usage.ru_maxrss);
                printf("Input/output operations: %ld \n", usage.ru_inblock + usage.ru_oublock);
                printf("Systemwide measured energy in microjoules %"PRIu64 "\n", total_energy_used);
            }
            /* */

        } else {
            // fork failed
            printf("Fork failed\n");
            exit(1);
        }

        return 0;
    }
    // -m monitoring given processes by id
    else if (strcmp(argv[1], "-m") == 0)
    {
        int num_processes = argc - 2; // number of processes to monitor
        struct proc_info *processes = malloc(num_processes * sizeof(struct proc_info)); // allocate memory
        // iterate through arguments and store process id
        for (int i = 0; i < num_processes; i++)
        { 
            processes[i].pid = (pid_t) atoi(argv[i+2]);
            processes[i].cputime = 0;
            processes[i].rss = 0;
            processes[i].io_op = 0;
            processes[i].energy_interval_est = 0;
        }

        // loop in time interval to update / remove ended processes
        while (num_processes > 0) 
        {
            energy_before_pkg = read_energy(0);
            energy_before_dram = read_energy(3);
            for (int i = 0; i < num_processes; i++)
            {
                int ret = read_process_stats(&processes[i]);
                if (ret == -1) 
                {
                    // process has terminated, remove it from the array
                    if (num_processes==1) { // all processes terminated
                        return 0;
                    }
                    for (int j = i; j < num_processes - 1; j++) {
                        processes[j] = processes[j + 1];
                    }
                    num_processes--;
                    i--; // correct current index
                }
            }
            // TODO output
            for (int i = 0; i < num_processes; i++) {
                print_pinfo(&processes[i]);
            }
            sleep(interval);
            energy_after_pkg = read_energy(0);
            energy_after_dram = read_energy(3);
            total_energy_used = check_overflow(energy_before_pkg, energy_after_pkg)
                                + check_overflow(energy_before_dram, energy_after_dram);
            printf("Total used energy in timeinterval(%d) in microjoules: %"PRIu64 "\n", 
                interval, total_energy_used);
        }
        free(processes);
    }

    // -c calibration of estimations
    else if (strcmp(argv[1], "-c") == 0)
    {
        // TODO ////
        // calibration
        // TODO ////
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


