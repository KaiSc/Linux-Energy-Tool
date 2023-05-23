#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ///////////////////////////////////////////
   using cgroups v2 /sys/fs/cgroup/system.slice contains
   the docker container cgroups, named docker-*ID*.scope,
   /cpu.stat, /io.stat, /mem.stat or /mem.current have statistics
   /cgroup.procs + /cgroup.threads lists pids of the processes
*/ ///////////////////////////////////////////
int get_containers() {
    // open cgroup directory
    char *dir_path = "/sys/fs/cgroup/system.slice";
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        // TODO v1 directory
        perror("No cgroup v2");
    }

    // walk through directory
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // sub directory with name "docker-X" where x is container ID
        if (entry->d_type == DT_DIR && strncmp(entry->d_name, "docker-", 7) == 0) {
            // container cgroup
            char *id_str = entry->d_name + 7;
            // TODO TODO
            /////////// testing
            printf("Container %s found. \n", entry->d_name + 7);
            printf("Includes processes: \n");
            FILE *fp;
            int eof;
            char path[256];
            snprintf(path, sizeof(path), "/sys/fs/cgroup/system.slice/docker-%s/cgroup.procs", id_str);
            fp = fopen(path, "r");
            if (!fp) {
                perror("Couldn't open /cgroups.procs file");
                return -1;
            }
            // check for end of file
            while ((eof = fgetc(fp)) != EOF) {
                printf("%c", eof);
            }
            fclose(fp);

            // get entire container stats
            // TODO 
            // monitor the individual container processes
            // TODO
            ///////////

        }
    }
    closedir(dir);  
    // get stats 
}

 /* testing
int main(int argc, char const *argv[])
{   
    get_containers();
    return 0;
}
/**/