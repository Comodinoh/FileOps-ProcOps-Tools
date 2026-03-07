#!/bin/bash

START=$(date)

mkdir -p reports/fs reports/process/ reports/proc reports/pipeline

# Filesystem
ls -la > reports/fs/A1_ls_long.txt
find -name "*.sh" -fprint reports/fs/A2_find_sh.txt
du --exclude *.git* -h -d 1 > reports/fs/A3_du_level1.txt

# Processes
LINES=17 top -b -n1 -o %MEM -w > reports/process/B1_top_mem.txt
pstree -g > reports/process/B2_pstree.txt
(sleep 60 &) && pgrep -l sleep > reports/process/B3_pgrep_sleep.txt

# /proc
grep "model name" /proc/cpuinfo | uniq | cut -f2 -d: | tail -c +2 > reports/proc/C1_cpu_model.txt
grep -E "MemTotal|MemAvailable" /proc/meminfo | cut -f2 -d: | tr -d ' ' > reports/proc/C2_mem_total_avail.txt
cat /proc/uptime | cut -f1 -d' ' > reports/proc/C3_uptime.txt

# Pipelines
find -type f ! -wholename *.git/* -printf "%sB %p\n" | sort --field-separator=' ' -k 1n | tail -n 5 > reports/pipeline/D1_top5_large_files.txt
ps --delimiter ' ' -e --format size,pid,cmd | sort -t' ' -k3 -u | sort -t' ' -k1n | tail -n5 | cut -f2,3- -d' ' > reports/pipeline/D2_proc_mem_pid_name.txt
grep "Command: " doc/T1_commands.md | sort | wc -l > reports/pipeline/D3_count_commands.txt

END=$(date)

echo "Started At: ${START}" > reports/T1_summary.txt
echo "Ended At: ${END}" >> reports/T1_summary.txt
echo "Written to files: " >> reports/T1_summary.txt
echo " - reports/fs/A1_ls_long.txt" >> reports/T1_summary.txt
echo " - reports/fs/A2_find_sh.txt" >> reports/T1_summary.txt
echo " - reports/fs/A3_du_level1.txt" >> reports/T1_summary.txt
echo " - reports/process/B1_top_mem.txt" >> reports/T1_summary.txt
echo " - reports/process/B2_pstree.txt" >> reports/T1_summary.txt
echo " - reports/process/B3_pgrep_sleep.txt" >> reports/T1_summary.txt
echo " - reports/proc/C1_cpu_model.txt" >> reports/T1_summary.txt
echo " - reports/proc/C2_mem_total_avail.txt" >> reports/T1_summary.txt
echo " - reports/proc/C3_uptime.txt" >> reports/T1_summary.txt
echo " - reports/pipeline/D1_top5_large_files.txt" >> reports/T1_summary.txt
echo " - reports/pipeline/D2_proc_mem_pid_name.txt" >> reports/T1_summary.txt
echo " - reports/pipeline/D3_count_commands.txt" >> reports/T1_summary.txt

