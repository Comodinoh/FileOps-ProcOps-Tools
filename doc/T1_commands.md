# Filesystem commands (A)
## ls long (A1)
 - Command: `ls -la > reports/fs/A1_ls_long.txt`
 - Output: `reports/fs/A1_ls_long.txt`
 - Description: `Outputs the contents of the current working directory with the long format, including hidden files`
## find sh (A2)
 - Command: `find -name "*.sh" -f reports/fs/A2_find_sh.txt`
 - Output: `reports/fs/A2_find_sh.txt`
 - Description: `Searches for all files ending with .sh and outputs their relative path`
## du level1 (A3)
 - Command: `du --exclude *.git* -h -d 1 > reports/fs/A3_du_level1.txt`
 - Output: `reports/fs/A3_du_level.txt`
 - Description: `Outputs the human-readable size of all level 1 directories`
# Process commands (B)
## top mem (B1)
 - Command: `LINES=17 top -b -n1 -o %MEM -w > reports/process/B1_top_mem.txt`
 - Output: `reports/process/B1_top_mem.txt`
 - Description: `Outputs top 10 processes sorted in descending order by memory consumption`
## pstree (B2)
 - Command: `pstree -g > reports/process/B2_pstree.txt`
 - Output: `reports/process/B2_pstree.txt`
 - Description: `Outputs the process tree with visible PIDs`
## pgrep sleep (B3)
 - Command: `(sleep 60 &) && pgrep -l sleep > reports/process/B3_pgrep_sleep.txt`
 - Output: `reports/process/B3_pgrep_sleep.txt`
 - Description: `Creates a test process and outputs its PID and name identified by the name`
 - Note: `To stop the resulting process the command killall sleep or kill <pid> should be executed, where <pid> is the outputted pid of the process`

# /proc commands (C)
## cpu model (C1)
 - Command: `grep "model name" /proc/cpuinfo | uniq | cut -f2 -d: | tail -c +2 > reports/proc/C1_cpu_model.txt`
 - Output: `reports/proc/C1_cpu_model.txt`
 - Description: `Searches for the CPU model name from /proc/cpuinfo, formats it, deletes leading whitespace and outputs it correctly`
## mem total avail (C2)
 - Command: `grep -E "MemTotal|MemAvailable" /proc/meminfo | cut -f2 -d: | tr -d ' ' > reports/proc/C2_mem_total_avail.txt`
 - Output: `reports/proc/C2_mem_total_avail.txt`
 - Description: `Searches for total memory and available memory from /proc/meminfo, formats it, deletes leading whitespace and outputs it correctly`
## uptime (C3)
 - Command: `cat /proc/uptime | cut -f1 -d' ' > reports/proc/C3_uptime.txt`
 - Output: `reports/proc/C3_uptime.txt`
 - Description: `Outputs the system uptime from /proc/uptime`

# Pipeline commands (D)
## top5 large files (D1)
 - Command: `find -type f ! -wholename *.git/* -printf "%sB %p\n" | sort --field-separator=' ' -k 1n | tail -n 5 > reports/pipeline/D1_top5_large_files.txt`
 - Output: `reports/pipeline/D1_top5_large_files.txt`
 - Description: `Searches for all the files in the project, formats it to size and file path, sorts it by size, and outputs the top 5`
## top5 proc mem pid name (D2)
 - Command: `ps --delimiter ' ' -e --format size,pid,cmd | sort -t' ' -k3 -u | sort -t' ' -k1n | tail -n5 | cut -f2,3- -d' ' > reports/pipeline/D2_top5_proc_mem_pid_name.txt`
 - Output: `reports/pipeline/D2_top5_proc_mem_pid_name.txt`
 - Description: `Selects all processes formatted by size, pid and name, sorts them by name to eliminate duplicates, sorts them by used memory, trims it to the top 5 and outputs only process pid and name`
## count commands (D3)
 - Command: `grep "Command: " doc/T1_commands.md | sort | wc -l > reports/pipeline/D3_count_commands.txt`
 - Output: `reports/pipeline/D3_count_commands.txt` 
 - Description: `Searches for lines that contain commands in doc/T1_commands.md, sorts them and outputs their count`

