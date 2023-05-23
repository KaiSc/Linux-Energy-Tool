#include <stdio.h>
#include <stdlib.h>
#include <nvml.h>
#include <inttypes.h>

// Initialize NVML and GPU device count
int initialize_gpu(unsigned int *device_count){
    nvmlReturn_t result;

    // Initialize NVML
    result = nvmlInit();
    if (result != NVML_SUCCESS) {
        printf("Failed to initialize NVML: %s\n", nvmlErrorString(result));
        return 1;
    }

    // Get NVIDIA GPU count
    result = nvmlDeviceGetCount(device_count);
    if (result != NVML_SUCCESS) {
        printf("Failed to get device count: %s\n", nvmlErrorString(result));
        return 1;
    }
}

int get_static_info(unsigned int device_count, nvmlDevice_t *handle_array,
                     unsigned int *max_power_array) {

    nvmlReturn_t result;
    nvmlDevice_t device;
    unsigned int i, max_power, power_min, max_power_constraint;
        
    // Get power limit of each NVIDIA GPU
    for (i = 0; i < device_count; i++) {
        result = nvmlDeviceGetHandleByIndex(i, &device);
        if (result != NVML_SUCCESS) {
            printf("Failed to get device handle: %s\n", nvmlErrorString(result));
            return 1;
        }
        handle_array[i] = device;

        // Get the maximum power of GPU
        result = nvmlDeviceGetPowerManagementLimitConstraints(device, &power_min, &max_power);
        if (result != NVML_SUCCESS) {
            printf("Failed to get power limit constraints: %s\n", nvmlErrorString(result));
            return 1;
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
        /**/
    }
    return 0;
}

// Get current GPU usage
int get_gpu_info(unsigned int device_count, nvmlDevice_t *handle_array,
    nvmlUtilization_t *utilization_array, nvmlMemory_t *memory_array, 
    unsigned int *fan_speed_array, unsigned int *temperature_array){

    nvmlReturn_t result;
    unsigned int i;
    nvmlDevice_t device;
    nvmlUtilization_t utilization;
    nvmlMemory_t memory;
    unsigned int temperature, fan_speed;

    // Loop through all NVIDIA GPUs
    for (i = 0; i < device_count; i++) {

        // Get GPU utilization over last sampling period between 1/6 to 1sec
        result = nvmlDeviceGetUtilizationRates(handle_array[i], &utilization);
        if (result != NVML_SUCCESS) {
            printf("Failed to get utilization for device %u: %s\n", i, nvmlErrorString(result));
            return 1;
        }
        utilization_array[i] = utilization;
        printf("GPU Device %u, utilization: %u%%\n", i, utilization_array[i].gpu);
        printf("GPU Device %u, memory utilization: %u%%\n", i, utilization_array[i].memory);

        // Get GPU memory usage
        result = nvmlDeviceGetMemoryInfo(handle_array[i], &memory);
        if (result != NVML_SUCCESS) {
            printf("Failed to get memory info: %s\n", nvmlErrorString(result));
            return 1;
        }
        memory_array[i] = memory;
        printf("GPU Device %u, Total GPU Memory: %llu bytes\n", i, memory_array[i].total);
        printf("GPU Device %u, Used GPU Memory: %llu bytes\n", i, memory_array[i].used);
        printf("GPU Device %u, Free GPU Memory: %llu bytes\n", i, memory_array[i].free);

        // Get GPU fan speed
        result = nvmlDeviceGetFanSpeed(handle_array[i], &fan_speed);
        if (result != NVML_SUCCESS) {
            printf("Failed to get GPU fan speed: %s\n", nvmlErrorString(result));
            return 1; 
        }
        fan_speed_array[i] = fan_speed;
        printf("GPU Device %u fan speed: %u %% \n", i, fan_speed_array[i]);

        // Get GPU temperature
        result = nvmlDeviceGetTemperature(handle_array[i], NVML_TEMPERATURE_GPU, &temperature);
        if (result != NVML_SUCCESS) {
            printf("Failed to get GPU temperature: %s\n", nvmlErrorString(result));
            return 1; 
        }
        temperature_array[i] = temperature;
        printf("GPU Device %u temperature: %u degrees Celsius\n", i, temperature_array[i]);
    }
}


/* // testing
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

    // TODO multiple GPUs which handle to read
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
    // TODO insufficient size error
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
