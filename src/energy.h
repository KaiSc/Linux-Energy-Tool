#ifndef energy_h
#define energy_h

uint64_t read_energy(int domain);

uint64_t check_overflow(uint64_t before, uint64_t after);

int init_rapl();

double estimate_energy_cputime(unsigned long cputime, unsigned long cputime_proc,
        uint64_t energy_interval);
        
double estimate_energy_cycles(unsigned long cpu_cycles, long long cpu_cycles_proc,
        uint64_t energy_interval);

/* double estimate_energy(unsigned long cputime, long rss, long io_op, long long cpu_cycles, 
        unsigned long cputime_proc, long rss_proc, long io_op_proc, long long cpu_cycles_proc,
        uint64_t energy_interval);
        */

#endif
