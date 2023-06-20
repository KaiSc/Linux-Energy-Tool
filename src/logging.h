#ifndef logging_h
#define logging_h

FILE* initLogFile();

int writeToFile(FILE *fp, char* buffer);

int system_stats_to_buffer(struct system_stats *system_stats, long long energy, char* buffer);

int process_stats_to_buffer(struct proc_stats *proc_stats, char* buffer);

int container_stats_to_buffer(struct container_stats *container_stats, char* buffer);

int e_stats_to_buffer(double cpu_time, long max_rss, long io, long long cycles, long long energy, char* buffer);


#endif