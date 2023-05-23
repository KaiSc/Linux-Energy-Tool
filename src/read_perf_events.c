#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>

#define buffersize 256

// open perf event, requires /proc/sys/kernel/perf_event_paranoid to be 0
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

// TODO extend from energy to other statistics 
// list of power events in /sys/devices/power/events/ with scale and unit
int main(int argc, char **argv)
{

    FILE *fd;
    int type;
    int config;
    char units[buffersize];
    double scale;
    struct perf_event_attr pe;
    long long counter;
    int ret;

    // check power event type
    fd = fopen("/sys/bus/event_source/devices/power/type","r");
    if (fd == NULL) {
        printf("No RAPL event support \n");
    }
    fscanf(fd, "%d", &type);
    fclose(fd);

    // check rapl event id
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-pkg", "r");
    fscanf(fd, "event=%x", &config);
    fclose(fd);

    // check scale
    fd = fopen("/sys/bus/event_source/devices/power/events/energy-pkg.scale", "r");
    fscanf(fd,"%lf",&scale);
    fclose(fd);

    // check units? (should always be joules)
    // open perf event
    memset(&pe, 0x0, sizeof(pe));
    pe.type = type;
    pe.config = config;

    ret = perf_event_open(&pe, -1, 0, -1, 0);
    if (ret<0) {
        printf("Error opening perf event \n");
    }

    // workload
    sleep(10);
    //

    read(ret, &counter, 8);
    close(ret);
    printf("Energy consumed %lf \n", (double) counter*scale);
}
