#ifndef container_stats_h
#define container_stats_h

#define MAX_CONTAINERS 25

struct container_stats { 
    char id[256];
    unsigned long long cputime; // in microseconds
    long long memory; // in bytes
    unsigned long io_op;
    // TODO process ids
    unsigned long cputime_interval; // in microseconds
    long long memory_interval; // in bytes
    long io_op_interval;
    unsigned long long cycles_interval;
    long long energy_interval_est; // in microjoules
};

extern struct container_stats containers[MAX_CONTAINERS];

extern int num_containers;

int init_docker_container();

int get_docker_containers();

int update_docker_containers();


#endif
