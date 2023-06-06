#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <asm/unistd.h>

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

int setUpProcCycles(pid_t pid ) {
    struct perf_event_attr pe;
    int fd;

    // Create event attribute
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.config = PERF_COUNT_HW_CPU_CYCLES;  // Measure CPU cycles
    pe.disabled = 1;  // Start the counter in a disabled state
    pe.exclude_kernel = 0;  // Include kernel space measurement
    pe.exclude_hv = 1;  // Exclude hypervisor from measurement
    pe.size = sizeof(struct perf_event_attr);

    // Open event counter
    fd = perf_event_open(&pe, pid, -1, -1, 0);
    if (fd == -1) {
        printf("Error opening perf event\n");
        return 1;
    }

    // Clear and enable event counter
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    return fd;
}

long long readInterval(int fd) {
    long long counter;
    // Read counting event counter
    read(fd, &counter, sizeof(long long));
    // Reset counter, or implement overflow 
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    //printf("CPU cycles: %llu\n", counter);
}

int closeEvent(int fd) {
    // Disable the counter and read the counter value
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);

    // Close the perf event
    close(fd);
}

/* testing
int main(int argc, char **argv)
{
    // monitor given process on any cpu
    int fd;
    pid_t pid = 0; // process id
    fd = setUpProcCycles(pid);
    while(1) {
        sleep(1);
        readInterval(fd);
    }
    closeEvent(fd);
}
// */

static int energy_event_type;
static double pkg_scale;
static double ram_scale;
static int pkg_config;
static int ram_config;

// return 0 if both, 1 if only package, -1 if no support
int initEnergy() {
    FILE *fd;
    // check power event type
    fd = fopen("/sys/bus/event_source/devices/power/type","r");
    if (fd == NULL) { // if no RAPL support
        printf("No RAPL event support \n");
        return -1;
    }
    fscanf(fd, "%d", &energy_event_type);
    fclose(fd);

    // check rapl pkg event id
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-pkg", "r");
    fscanf(fd, "event=%x", &pkg_config);
    fclose(fd);

    // check scale
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-pkg.scale", "r");
    fscanf(fd,"%lf",&pkg_scale);
    fclose(fd);

    // check rapl ram event id
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-ram", "r");
    if (fd==NULL){ // if ram not supported
        return 1;
    }
    fscanf(fd, "event=%x", &ram_config);
    fclose(fd);

    // check scale
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-ram.scale", "r");
    fscanf(fd,"%lf",&ram_scale);
    fclose(fd);

    return 0;
}

int openPkgEvent() {
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0x0, sizeof(pe));
    pe.type = energy_event_type;
    pe.config = pkg_config;
    fd = perf_event_open(&pe, -1, 0, -1, 0);
    if (fd==-1) {
        printf("Error opening energy-pkg event \n");
    }

    // Clear and enable event counter
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    return fd;
}

int openRamEvent() {
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0x0, sizeof(pe));
    pe.type = energy_event_type;
    pe.config = ram_config;
    fd = perf_event_open(&pe, -1, 0, -1, 0);
    if (fd==-1) {
        printf("Error opening energy-pkg event \n");
    }

    // Clear and enable event counter
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);

    return fd;
}

// type==0 -> pkg, type==1 -> dram;
double readEnergyInterval(int fd, int type) {
    long long counter;
    double energy_joules;
    // Read counting event counter
    read(fd, &counter, sizeof(long long));
    // Reset counter, or implement overflow 
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    
    if (type==0) {
        energy_joules = (double) counter*pkg_scale;
        printf("Pkg energy in Joules: %lf\n", energy_joules);
        return energy_joules;
    }
    else {
        energy_joules = (double) counter*ram_scale;
        printf("Ram energy in Joules: %lf\n", energy_joules);
        return energy_joules;
    }
}


/* testing 
int main(int argc, char **argv)
{
    int fd_pkg;
    int fd_ram;
    int supported;
    supported = initEnergy();
    fd_pkg = openPkgEvent();
    if (supported == 0) {
        fd_ram = openRamEvent();
    }
    while(1) {
        sleep(1);
        readEnergyInterval(fd_pkg, 0);
        if (supported==0) {
            readEnergyInterval(fd_ram, 1);
        }
    }
    closeEvent(fd_pkg);
    closeEvent(fd_ram);
}
// */