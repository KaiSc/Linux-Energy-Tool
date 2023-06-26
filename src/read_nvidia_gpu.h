#ifndef read_nvidia_gpu_h
#define read_nvidia_gpu_h

#include <nvml.h>

struct gpu_stats {
    nvmlDevice_t handle;
    unsigned int max_power;
    unsigned int min_power;
    unsigned int util;
    unsigned int mem_util;
    unsigned int fan_speed;
    unsigned int temperature;
};

int init_gpu();

int get_gpu_stats();

int gpu_stats_to_buffer(char* buffer);

#endif
