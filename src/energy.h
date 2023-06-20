#ifndef energy_h
#define energy_h

long long read_energy(int domain);

long long check_overflow(long long before, long long after);

int init_rapl();

long long estimate_energy_cycles(long long cpu_cycles, long long cpu_cycles_proc,
        long long energy_interval, double time);

/*
double estimate_energy_cputime(unsigned long cputime, unsigned long cputime_proc,
        long long energy_interval);
        

/* double estimate_energy(unsigned long cputime, long rss, long io_op, long long cpu_cycles, 
        unsigned long cputime_proc, long rss_proc, long io_op_proc, long long cpu_cycles_proc,
        long long energy_interval);
*/

#endif
