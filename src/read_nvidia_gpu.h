#ifndef read_nvidia_gpu_h
#define read_nvidia_gpu_h

#include <nvml.h>

struct gpu_stats {
    nvmlDevice_t handle;
    unsigned int max_power; // in watts
    unsigned int min_power; // in watts
    unsigned int util; // in %
    unsigned int mem_util; // in %
    unsigned int fan_speed; // in %
    unsigned int temperature; // in %
};

int init_gpu();

long long get_gpu_stats();

int gpu_stats_to_buffer(char* buffer);

void print_gpu_stats();


#endif
