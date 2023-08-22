#ifndef logging_h
#define logging_h

FILE* initLogFile();

FILE* initBenchLogFile(char* alg_name, char* lang_name);

int writeToFile(FILE *fp, char* buffer);

int system_stats_to_buffer(struct system_stats *system_stats, long long energy, char* buffer);

int process_stats_to_buffer(struct proc_stats *proc_stats, char* buffer);

int container_stats_to_buffer(struct container_stats *container_stats, char* buffer);

int e_stats_to_buffer(double cpu_time, long max_rss, long io, long long cycles, long long energy, char* buffer);

int system_interval_to_buffer(struct system_stats *s_stats, long long energy, char* buffer);

int system_interval_gpu_to_buffer(struct system_stats *s_stats, long long energy, long long gpu_energy, char* buffer);

int cgroup_stats_to_buffer(struct cgroup_stats *c_stats, double time, char* buffer);

#endif
