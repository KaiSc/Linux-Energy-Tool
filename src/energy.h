#ifndef energy_h
#define energy_h

uint64_t read_energy(int domain);

uint64_t check_overflow(uint64_t before, uint64_t after);

int init_rapl();

uint64_t estimate_energy(unsigned long cputime, long rss, long io_op);


#endif
