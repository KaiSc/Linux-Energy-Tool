#ifndef perf_events_h
#define perf_events_h

int setUpProcCycles(pid_t pid);

long long readInterval(int fd);

int initEnergy();

int openPkgEvent();

int openRamEvent();

double readEnergyInterval(int fd, int i);

int closeEvent(int fd);


#endif
