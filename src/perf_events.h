#ifndef perf_events_h
#define perf_events_h

#include <sys/types.h>

int setUpProcCycles(pid_t pid);

int setUpProcCycles_cpu(int cpu);

int setUpProcCycles_cgroup(int cgroup_fd, int cpu);

long long readInterval(int fd);

int initEnergy();

int openPkgEvent();

int openRamEvent();

double readEnergyInterval(int fd, int i);

int closeEvent(int fd);


#endif
