#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "perf_events.h"
#include "energy.h"

/* ///////////////////////////////////////////
   using cgroups v2 /sys/fs/cgroup/system.slice contains
   the docker container cgroups, named docker-*ID*.scope,
   /cpu.stat, /io.stat, /memory.stat or /memory.current have statistics
   /cgroup.procs + /cgroup.threads lists pids of the processes
*/ ///////////////////////////////////////////

// TODO
// - check cgroup.controllers to make sure cpu, memory and io are supported?
// - use hashmap to store containers for efficiency
// - open perf event as group event?

#define MAX_CONTAINERS 25

struct container_stats { 
    char id[256];
    unsigned long long cputime; // in microseconds
    long long memory; // in bytes
    unsigned long io_op;
    // TODO process ids
    unsigned long cputime_interval;
    long long memory_interval;
    long io_op_interval;
    unsigned long long cycles_interval;
    long long energy_interval_est; // in microjoules
};

struct container_stats containers[MAX_CONTAINERS]; // Array to store container information
static int *cgroup_perf_fds;
int num_containers = 0; // Number of containers currently stored
int max_cpus = 0;

int add_docker_container(char *id);
int remove_docker_container(int i);

int init_docker_container() {
    max_cpus = sysconf(_SC_NPROCESSORS_CONF);
    int size = max_cpus * MAX_CONTAINERS;
    cgroup_perf_fds = malloc(sizeof(int) * size);
    if (cgroup_perf_fds == NULL) {
        printf("Container perf event array allocation failed.\n");
        return -1;
    }
    return 0;
}

int get_docker_containers() {
    // open cgroup directory
    char *dir_path = "/sys/fs/cgroup/system.slice";
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("No cgroup v2");
    }
    // walk through directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Sub directory with name "docker-X" where x is container ID
        if (strncmp(entry->d_name, "docker-", 7) == 0) {
            char *id_str = entry->d_name + 7;
            size_t id_length = strlen(id_str);
            id_str[id_length - 6] = '\0'; // removing ".scope"
            add_docker_container(id_str);
        }
    }
    closedir(dir);
    
    return 0;
}

int update_docker_containers() {
    char path[512];
    char line[256];
    FILE *fp;
    // Update current containers
    for (int i = 0; i < num_containers; i++)
    {
        // Update and remove terminated containers
        // Cpu stats
        snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/cpu.stat",
                                    containers[i].id);
        fp = fopen(path, "r");
        if (fp == NULL) {
            perror("Couldn't open /cpu.stat file\n");
            // Remove container, adjust loop
            remove_docker_container(i);
            i--;
            continue;
        }
        // Read the cpu.stat file
        unsigned long long usage_usec;
        fscanf(fp, "%*s %llu", &usage_usec);
        containers[i].cputime_interval = usage_usec - containers[i].cputime;
        containers[i].cputime = usage_usec;
        //printf("Container CPU usage in microseconds (interval): %lu\n", containers[i].cputime_interval);
        fclose(fp);

        // IO stats
        snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/io.stat",
                                    containers[i].id);
        fp = fopen(path, "r");
        if (fp == NULL) {
            perror("Couldn't open /io.stat file");
            // Remove container, adjust loop
            remove_docker_container(i);
            i--;
            continue;
        }
        // Read the io.stat file
        unsigned long long rbytes = 0, wbytes = 0, rios = 0, wios = 0, ios = 0;
        if (fgets(line, sizeof(line), fp)) {
            sscanf(line,"%*d:%*d rbytes=%llu wbytes=%llu rios=%llu wios=%llu",&rbytes,&wbytes,&rios,&wios);
        }
        fclose(fp);
        ios = rios + wios;
        containers[i].io_op_interval = ios - containers[i].io_op;
        containers[i].io_op = ios;
        //printf("Container IO operations in interval: %ld \n", containers[i].io_op_interval);

        // Memory stats
        snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/memory.current",
                                    containers[i].id);
        fp = fopen(path, "r");
        if (fp == NULL) {
            // Remove container, adjust loop
            remove_docker_container(i);
            i--;
            continue;
        }
        // Read the memory.current file
        long long cur_memory = 0;
        fscanf(fp, "%lld", &cur_memory);
        containers[i].memory_interval = containers[i].memory - cur_memory;
        containers[i].memory = cur_memory;
        //printf("Container Memory change: %llu\n", containers[i].memory_interval);
        fclose(fp);

        // Read perf events
        long long cgroup_cycles = 0;
        int offset = max_cpus*i;
        for (int j = 0; j < max_cpus; j++)
        {
            cgroup_cycles += readInterval(cgroup_perf_fds[j+offset]);
        }
        containers[i].cycles_interval = cgroup_cycles;
        
    }
    // Check directory to add new ones
    char *dir_path = "/sys/fs/cgroup/system.slice";
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("No cgroup v2");
    }

    // Walk through directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        int already_monitored = 0;
        // Sub directory with name "docker-X" where x is container ID
        if (strncmp(entry->d_name, "docker-", 7) == 0) {
            char *id_str = entry->d_name + 7;
            size_t id_length = strlen(id_str);
            id_str[id_length - 6] = '\0'; // removing ".scope"

            for (int j = 0; j < num_containers; j++) {
                if (strcmp(containers[j].id, id_str) == 0) {
                    // Container is already being monitored
                    already_monitored = 1;
                    break;
                }
            }
            if (already_monitored == 1) {
                continue;
            }
            // Add new container to monitor
            add_docker_container(id_str);
        }
    }
    closedir(dir);
}

int add_docker_container(char *id_str) {
    if (num_containers == MAX_CONTAINERS) {
        return -1;
    }
    char line[256];
    struct container_stats container;
    strcpy(container.id, id_str);
    container.cputime = 0;
    container.memory = 0;
    container.io_op = 0;
    container.cputime_interval = 0;
    container.memory_interval = 0;
    container.io_op_interval = 0;
    container.energy_interval_est = 0;
    // Container cgroup
    printf("Adding Container %s \n", id_str);
    FILE *fp;
    char path[512];
    /*// Check container process ids
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/cgroup.procs", id);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Couldn't open /cgroups.procs file");
        return -1;
    }
    printf("Includes processes: \n");
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("%s", line);
    }
    fclose(fp);*/

    // Get entire container stats
    // Cpu stats
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/cpu.stat", id_str);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Couldn't open /cpu.stat file, addcontainer");
        return -1;
    }
    // Read the cpu.stats file
    unsigned long long usage_usec;
    fscanf(fp, "%*s %llu", &container.cputime);
    fclose(fp);

    // IO stats
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/io.stat", id_str);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Couldn't open /io.stat file, addcontainer");
        return -1;
    }
    // Read the io.stat file
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
    container.io_op = ios;

    // Memory stats
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/memory.current", id_str);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Couldn't open /memory.current file, addcontainer");
        return -1;
    }
    // Read the memory.current file
    fscanf(fp, "%lld", &container.memory);
    fclose(fp);
    
    /*
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/memory.stat", id_str);
    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("Couldn't open /memory.current file");
        return 1;
    }
    // TODO read memory.stats file? anon + file + kernel = mem.current
    // has additional stats
    fclose(fp);
    */

    // Open perf events for cgroup
    snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s.scope/", id_str);
    int fd = open(path, O_RDONLY);
    int offset = num_containers*max_cpus;
    for (int j = 0; j < max_cpus; j++)
    {
        cgroup_perf_fds[offset+j] = setUpProcCycles_cgroup(fd, j);
    }
    close(fd);

    containers[num_containers] = container;
    num_containers++;

    return 0;
}

int remove_docker_container (int i) {
    // Close associated perf events
    int offset = i*max_cpus;
    for (int j = 0; j < max_cpus; j++)
    {
        closeEvent(cgroup_perf_fds[offset+j]);
        cgroup_perf_fds[offset+j] = cgroup_perf_fds[num_containers+j];
    }
    
    // Remove container
    printf("Removing container %s\n", containers[i].id);
    containers[i] = containers[num_containers-1];
    num_containers--;

    return 0;
}



/* testing
int main(int argc, char const *argv[])
{   
    get_containers();
    return 0;
}
/**/