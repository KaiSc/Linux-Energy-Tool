#ifndef benchmarking_h
#define benchmarking_h

extern pid_t cgroup_id;

struct cgroup_stats { 
    unsigned long long cputime; // in microseconds
    long long maxRSS; // in bytes
    unsigned long io_op;
    unsigned long long cycles;
    long long estimated_energy; // in microjoules
};

int init_benchmarking();

int reset_cgroup();

int read_cgroup_stats(struct cgroup_stats *cg_stats);

int close_cgroup();

#endif
