#ifndef process_stats_h
#define process_stats_h

#include <inttypes.h>

struct proc_info 
{ 
    pid_t pid;
    unsigned long cputime; // in clock ticks, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kilobytes
    long io_op; 
    unsigned long cputime_interval;
    long rss_interval;
    long io_op_interval;
    long long cycles_interval;
    int fd;
    double energy_interval_est; // in microjoules
};


struct system_stats 
{
    unsigned long cputime; // in clock ticks, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kilobytes
    long io_op;
    unsigned long cputime_interval;
    long rss_interval;
    long io_op_interval;
    long long cycles;
};

int read_process_stats(struct proc_info *p_info);

int read_systemwide_stats(struct system_stats *sys_stats);

int check_zombie_state(pid_t pid);

#endif
