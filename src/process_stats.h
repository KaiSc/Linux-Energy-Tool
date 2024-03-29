#ifndef process_stats_h
#define process_stats_h

struct proc_stats 
{ 
    pid_t pid;
    unsigned long cputime; // in jiffies, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kB
    long io_op; 
    unsigned long cputime_interval; // in jiffies
    long rss_interval; // in kB
    long io_op_interval;
    long long cycles_interval;
    int fd;
    long long energy_interval_est; // in microjoules
};


struct system_stats 
{
    unsigned long cputime; // in jiffies, divide by sysconf(_SC_CLK_TCK) for seconds
    long rss; // in kB
    long io_op;
    unsigned long cputime_interval; // in jiffies
    long rss_interval; // in kB
    long io_op_interval;
    long long cycles;
};

int read_process_stats(struct proc_stats *p_info);

int read_systemwide_stats(struct system_stats *sys_stats);

int check_zombie_state(pid_t pid);

#endif
