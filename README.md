# Linux-Energy-Tool
Tool to estimate energy consumption of processes on Linux  
Requires:  
powercap, RAPL CPU supported by powercap (Intel CPU or AMD 17h/19h)  
/proc/ pseudo filesystem on Linux  
Cgroups V2 for Docker container monitoring  
optionally Nvidia GPU and NVML library installed  
(Still a work in progress)  
compile without NVML:  
gcc main.c container_stats.c energy.c perf_events.c process_stats.c logging.c -o main  
compile with NVML:  
gcc main_nvml.c container_stats.c energy.c perf_events.c process_stats.c logging.c read_nvidia_gpu.c -o main_nvml -lnvidia-ml  