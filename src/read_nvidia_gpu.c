#include <stdio.h>
#include <nvml.h>
#include <inttypes.h>

// TODO power/energy for individual process

int read_gpu() {
    nvmlDevice_t device;
    nvmlReturn_t result;
    unsigned int microwatts; // current power
    unsigned long long millijoules; // total energy

    // init NVML
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

    // shutdown NVML
    nvmlShutdown();

    printf("Measured power in microwatts: %i \n", microwatts);
    printf("Measured total energy in millijoules: %llu", millijoules)

}