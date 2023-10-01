#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

static nvmlReturn_t result;

static struct gpu_stats *GPU_stats;
static unsigned int gpu_device_count;

struct gpu_stats {
    nvmlDevice_t handle;
    unsigned int max_power; // in miliwatts
    unsigned int min_power; // in miliwatts
    unsigned int util; // in % of max over last sample period
    unsigned int mem_util; // in % of max over last sample period
    unsigned int fan_speed; // in % of max currently
    unsigned int temperature; // in °C
};

// Initialize NVML and GPU device count
int init_gpu() {

    // Initialize NVML
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
        return -1;
    }

    // Get NVIDIA GPU device count
    result = nvmlDeviceGetCount(&gpu_device_count);
    if (result != NVML_SUCCESS) {
        printf("Failed to get device count: %s\n", nvmlErrorString(result));
        return -1;
    }

    GPU_stats = malloc(sizeof(struct gpu_stats) * gpu_device_count);
    nvmlUtilization_t utilization;

    for (int i = 0; i < gpu_device_count; i++)
    {
        struct gpu_stats current = GPU_stats[i];
        result = nvmlDeviceGetHandleByIndex(i, &GPU_stats[i].handle);
        result = nvmlDeviceGetPowerManagementLimitConstraints(GPU_stats[i].handle,
                    &GPU_stats[i].min_power, &GPU_stats[i].max_power);
        printf("Min power mW:%u, Max power mW: %u \n", GPU_stats[i].min_power,  GPU_stats[i].max_power);
        result = nvmlDeviceGetUtilizationRates(GPU_stats[i].handle, &utilization);
        current.util = utilization.gpu;
        current.mem_util = utilization.memory;
        result = nvmlDeviceGetFanSpeed(GPU_stats[i].handle, &GPU_stats[i].fan_speed);
        result = nvmlDeviceGetTemperature(GPU_stats[i].handle, NVML_TEMPERATURE_GPU, 
                    &GPU_stats[i].temperature);
    }
    printf("GPU count %d\n", gpu_device_count);
}

// Get current GPU usage, estimate energy for 1 second, divide by actual interval
long long get_gpu_stats() {

    nvmlUtilization_t utilization;
    long long estimated_energy = 0;

    // Loop through all NVIDIA GPUs
    for (int i = 0; i < gpu_device_count; i++) {

        // Get GPU utilization over last sampling period between 1/6 to 1sec depending on device
        result = nvmlDeviceGetUtilizationRates(GPU_stats[i].handle, &utilization);
        if (result != NVML_SUCCESS) {
            printf("Failed to get utilization for device %u: %s\n", i, nvmlErrorString(result));
            return -1;
        }
        GPU_stats->util = utilization.gpu;
        GPU_stats->mem_util = utilization.memory;

        /* // Get GPU memory usage
        result = nvmlDeviceGetMemoryInfo(handle_array[i], &memory);
        if (result != NVML_SUCCESS) {
            printf("Failed to get memory info: %s\n", nvmlErrorString(result));
            return -1;
        }
        memory_array[i] = memory;
        printf("GPU Device %u, Total GPU Memory: %llu bytes\n", i, memory_array[i].total);
        printf("GPU Device %u, Used GPU Memory: %llu bytes\n", i, memory_array[i].used);
        printf("GPU Device %u, Free GPU Memory: %llu bytes\n", i, memory_array[i].free);
        */

        // Get GPU fan speed
        result = nvmlDeviceGetFanSpeed(GPU_stats[i].handle, &GPU_stats[i].fan_speed);
        if (result != NVML_SUCCESS) {
            printf("Failed to get GPU fan speed: %s\n", nvmlErrorString(result));
            return -1; 
        }

        // Get GPU temperature
        result = nvmlDeviceGetTemperature(GPU_stats[i].handle,
                                NVML_TEMPERATURE_GPU, &GPU_stats[i].temperature);
        if (result != NVML_SUCCESS) {
            printf("Failed to get GPU temperature: %s\n", nvmlErrorString(result));
            return -1; 
        }

        // Estimate GPU energy in microjoules (for 1/10th second), division into memory, computation and fan energy
        estimated_energy += round(
            (double) GPU_stats[i].util * (double) GPU_stats[i].max_power * 10.0 * 0.75 + // 75% computing
            (double) GPU_stats[i].mem_util * (double) GPU_stats[i].max_power * 10.0 * 0.15 + // 15% memory
            500000 + // 5 Watts idle
            200000 * GPU_stats[i].fan_speed // 2 Watts fan
            );

        // Print measurements
        //printf("GPU%d: util(%%):%u, mem_util(%%):%u, fan(%%):%u, temperature(°C):%u\n",
        //    i, utilization.gpu, utilization.memory, GPU_stats[i].fan_speed, GPU_stats[i].temperature);
        
        return estimated_energy;
    }
}

int gpu_stats_to_buffer(char* buffer) {
    char toString[256];

    for (int i = 0; i < gpu_device_count; i++)
    {   
        struct gpu_stats cur = GPU_stats[i];
        // device, max_power, min_power, util, mem_util, fan_speed, temperature
        sprintf(toString, "%u;%u;%u;%u;%u;%u;%u\n", i, cur.max_power, cur.min_power,
                        cur.util, cur.mem_util, cur.fan_speed, cur.temperature);
        strcat(buffer, toString);
        toString[0] = '\0';
    }
    
    return 0;
}

void print_gpu_stats() {
    for (int i = 0; i < gpu_device_count; i++) {
        printf("Current stats for GPU%d: util(%%):%u, mem_util(%%):%u, fan(%%):%u, temperature(°C):%u\n",
            i, GPU_stats[i].util, GPU_stats[i].mem_util, GPU_stats[i].fan_speed, GPU_stats[i].temperature);
    }
}

/* // testing


int get_static_info(unsigned int device_count, nvmlDevice_t *handle_array,
                     unsigned int *max_power_array) {

    nvmlDevice_t device;
    unsigned int max_power, power_min, max_power_constraint;
        
    // Get power limit of each NVIDIA GPU
    for (int i = 0; i < device_count; i++) {
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result != NVML_SUCCESS) {
            printf("Failed to get device handle: %s\n", nvmlErrorString(result));
            return -1;
        }
        handle_array[i] = device;

        // Get the maximum power of GPU
        result = nvmlDeviceGetPowerManagementLimitConstraints(device, &power_min, &max_power);
        if (result != NVML_SUCCESS) {
            printf("Failed to get power limit constraints: %s\n", nvmlErrorString(result));
            return -1;
        }
        printf("Device %d max power limit: %u watts\n", i, max_power / 1000);
        max_power_array[i] = max_power / 1000;

        /*
        // Get the maximum power constraint, system set limit?
        result = nvmlDeviceGetPowerManagementLimit(device, &max_power_constraint);
        if (result != NVML_SUCCESS) {
            printf("Failed to get current power limit: %s\n", nvmlErrorString(result));
            return 1;
        }
        printf("max power limit: %u watts\n", max_power_constraint / 1000);
        /**/ /*
    }
    return 0;
}



int main(int argc, char const *argv[]) {
    int gpu_count;
    int res;
    
    res = initialize_gpu(&gpu_count);

    // Dynamically allocate memory for arrays
    nvmlDevice_t* device_handle = malloc(sizeof(nvmlDevice_t) * gpu_count);
    unsigned int* max_power = malloc(sizeof(unsigned int) * gpu_count);
    nvmlUtilization_t* utilization = malloc(sizeof(nvmlUtilization_t) * gpu_count);
    nvmlMemory_t* memory = malloc(sizeof(nvmlMemory_t) * gpu_count);
    unsigned int* fan_speed = malloc(sizeof(unsigned int) * gpu_count);
    unsigned int* temperature = malloc(sizeof(unsigned int) * gpu_count);


    res = get_static_info(gpu_count, device_handle, max_power);
    while (1) {
        res = get_gpu_info(gpu_count, device_handle, utilization, memory, temperature, fan_speed);
        sleep(10);
        printf("---------------\n");
    }

    // Free dynamically allocated memory
    free(device_handle);
    free(max_power);
    free(utilization);
    free(memory);
    free(fan_speed);
    free(temperature);
    
    // Shutdown NVML
    nvmlShutdown();

    return 0;
}
/**/


/* 
--- Not supported by test GPU 


int read_gpu_energy() {
    nvmlDevice_t device;
    nvmlReturn_t result;
    unsigned int microwatts; // current power
    unsigned int millijoules; // total energy

    // Initialize NVML
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
        return 1;
    }

    // TODO multiple GPUs 
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        printf("Failed to get device handle: %s\n", nvmlErrorString(result));
        nvmlShutdown();
        return 1;
    }

    // Get power usage in milliwatts
    result = nvmlDeviceGetPowerUsage(device, &microwatts);
    if (result != NVML_SUCCESS) {
        printf("Failed to get current power usage: %s\n", nvmlErrorString(result));
        nvmlShutdown();
        return 1;
    }

    // Get total energy in millijoules
    result = nvmlDeviceGetPowerUsage(device, &millijoules);
    if (result != NVML_SUCCESS) {
        printf("Failed to get total energy consumption: %s\n", nvmlErrorString(result));
        nvmlShutdown();
        return 1;
    }

    // Shutdown NVML
    nvmlShutdown();

    printf("Measured power in microwatts: %u \n", microwatts);
    printf("Measured total energy in millijoules: %u", millijoules);

    return 0;
}


int gpu_processes() {
    nvmlReturn_t result;
    nvmlDevice_t device;
    unsigned int processSamplesCount = 0;
    nvmlProcessUtilizationSample_t *utilization = NULL;
    unsigned long long lastSeenTimeStamp = 0;
    
    // Initialize NVML
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
        return 1;
    }

    // Get handle for device 0
    result = nvmlDeviceGetHandleByIndex(0, &device);
    if (result != NVML_SUCCESS) {
        printf("Failed to get device handle: %s\n", nvmlErrorString(result));
        return 1;
    }

    // Determine size of buffer required to hold samples
    result = nvmlDeviceGetProcessUtilization(device, NULL, &processSamplesCount, lastSeenTimeStamp);
    if (result != NVML_SUCCESS) {
        printf("Failed to get process utilization1: %s\n", nvmlErrorString(result));
        return 1;
    }

    // Allocate buffer for samples
    utilization = (nvmlProcessUtilizationSample_t*)malloc(processSamplesCount * sizeof(nvmlProcessUtilizationSample_t));

    // Get process utilization samples
    result = nvmlDeviceGetProcessUtilization(device, utilization, &processSamplesCount, lastSeenTimeStamp);
    if (result != NVML_SUCCESS) {
        printf("Failed to get process utilization2: %s\n", nvmlErrorString(result));
        return 1;
    }

    // Print process utilization samples
    printf("Process utilization samples:\n");
    for (unsigned int i = 0; i < processSamplesCount; i++) {
        printf("Process ID: %u\n", utilization[i].pid);
        printf("GPU SM utilization: %u\n", utilization[i].smUtil);
        printf("GPU memory utilization: %u\n", utilization[i].memUtil);
        printf("GPU encoder utilization: %u\n", utilization[i].encUtil);
        printf("GPU decoder utilization: %u\n", utilization[i].decUtil);
        printf("CPU timestamp: %llu\n", utilization[i].timeStamp);
    }

    // Free memory allocated for samples
    free(utilization);
    // Shutdown NVML library
    nvmlShutdown();
    return 0;
}
/* */
