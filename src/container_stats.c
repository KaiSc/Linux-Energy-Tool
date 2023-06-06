#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>


/* ///////////////////////////////////////////
   using cgroups v2 /sys/fs/cgroup/system.slice contains
   the docker container cgroups, named docker-*ID*.scope,
   /cpu.stat, /io.stat, /memory.stat or /memory.current have statistics
   /cgroup.procs + /cgroup.threads lists pids of the processes
*/ ///////////////////////////////////////////

// TODO
// - check cgroup.controllers to make sure cpu, memory and io are supported?
// - storing/updating data and adding/removing containers

struct container_info { 
    char id[256];
    unsigned long cputime; // in microseconds
    long memory; // in kilobytes
    long io_op; 
    uint64_t energy_interval_est; // in microjoules
};


int get_containers() {
    // open cgroup directory
    char *dir_path = "/sys/fs/cgroup/system.slice";
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("No cgroup v2");
    }

    // walk through directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // sub directory with name "docker-X" where x is container ID
        if (entry->d_type == DT_DIR && strncmp(entry->d_name, "docker-", 7) == 0) {
            // container cgroup
            char *id_str = entry->d_name + 7;
            printf("Container %s found. \n", entry->d_name + 7);
            FILE *fp;
            char path[512];

            snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/cgroup.procs", id_str);
            fp = fopen(path, "r");
            if (fp == NULL) {
                perror("Couldn't open /cgroups.procs file");
                return 1;
            }
            // Read container processes
            char line[256];
            printf("Includes processes: \n");
            while (fgets(line, sizeof(line), fp) != NULL) {
                printf("%s", line);
                // TODO
                // track individual processes
            }
            fclose(fp);

            // Get entire container stats
            // Cpu stats
            snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/cpu.stat", id_str);
            fp = fopen(path, "r");
            if (fp == NULL) {
                perror("Couldn't open /cpu.stats file");
                return 1;
            }
            // Read the cpu.stats file
            unsigned long long usage_usec;
            fscanf(fp, "%*s %llu", &usage_usec);
            printf("Container CPU usage in microseconds: %llu\n", usage_usec);
            fclose(fp);

            // IO stats
            snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/io.stat", id_str);
            fp = fopen(path, "r");
            if (fp == NULL) {
                perror("Couldn't open /io.stats file");
                return 1;
            }
            // Read the io.stat file
            unsigned long long rbytes, wbytes, rios, wios;
            if (fgets(line, sizeof(line), fp)) {
                if (sscanf(line, "%*d:%*d rbytes=%llu wbytes=%llu rios=%llu wios=%llu", &rbytes, &wbytes, &rios, &wios) == 4) {
                    printf("Container Read Bytes: %llu\n", rbytes);
                    printf("Container Write Bytes: %llu\n", wbytes);
                    printf("Container Read I/Os: %llu\n", rios);
                    printf("Container Write I/Os: %llu\n", wios);
                }
            }

            fclose(fp);

            // Memory stats
            snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/memory.current", id_str);
            fp = fopen(path, "r");
            if (fp == NULL) {
                perror("Couldn't open /memory.current file");
                return 1;
            }
            // Read the memory.current file
            unsigned long long current_memory;
            fscanf(fp, "%llu", &current_memory);
            printf("Container Current Memory: %llu\n", current_memory);
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

        }
    }
    closedir(dir);
    
    return 0;
}

 /* testing
int main(int argc, char const *argv[])
{   
    get_containers();
    return 0;
}
/**/