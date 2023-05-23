#ifndef read_nvidia_gpu_h
#define read_nvidia_gpu_h

#include <nvml.h>

int initialize_gpu(unsigned int *device_count);

int get_static_info(unsigned int device_count, nvmlDevice_t *handle_array,
                     unsigned int *max_power_array);

int get_gpu_info(unsigned int device_count, nvmlDevice_t *handle_array,
    nvmlUtilization_t *utilization_array, nvmlMemory_t *memory_array, 
    unsigned int *fan_speed_array, unsigned int *temperature_array);

#endif
