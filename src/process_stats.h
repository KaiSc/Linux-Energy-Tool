#ifndef process_stats_h
#define process_stats_h

struct proc_info 
{ 
    pid_t pid;
    unsigned long cputime;
    long rss;
    long io_op;
    uint64_t energy_interval_est;
};

int read_process_stats(struct proc_info *p_info);

int read_systemwide_stats(struct proc_info *p_info);

int check_zombie_state(pid_t pid);

#endif
