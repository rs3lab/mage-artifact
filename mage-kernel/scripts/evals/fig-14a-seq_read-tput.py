#!/usr/bin/env python

import os
import sys
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

from logbox import LogBox

# ----------------
# PARAMETERS
# ----------------

bench_time = 30
page_size = 4 * 1024
output_path = Path('./csv/fig-14a-seq_read-tput.csv')
fig_path = Path('./fig/fig-14a-seq_read-tput.png')

# ----------------
# SCRIPT
# ----------------

# Scrape Logs!

logs = LogBox()

mind_root = os.environ.get('MIND_ROOT')
if mind_root is None:
    print('Error: $MIND_ROOT not set!', file=sys.stderr)
    sys.exit(1)

log_dir = Path(mind_root) / 'apps' / 'sequential-read'
if not log_dir.is_dir():
    print('Error: $MIND_ROOT/apps/sequential-read missing!', file=sys.stderr)
    sys.exit(1)

if logs.scrape_log_files(log_dir) == 0: 
    print('No log files found in $MIND_ROOT/apps/sequential-read missing!', file=sys.stderr)
    sys.exit(0)

# DEBUG

# Query Logs!

df = logs.get('''
         select fhthreads, sum(nr) as pages
         from summaries
         where 
             pp_name = 'FH_total'
             and cnthreads = 4
             and batch_size = 256
             and run = 1
        -- AKA: sum over only 'cpu' column
        group by pp_name, rid, fhthreads, cnthreads, batch_size, run
        order by fhthreads''')

df['tput_gibs'] = (df['pages'].astype(float) * page_size * 8 / 1e9) / bench_time
df = df[['fhthreads', 'tput_gibs']]

# write df to output path
print('Writing output CSV to', output_path)
output_path.parent.mkdir(parents=True, exist_ok=True)
df.to_csv(output_path, index=False)


print('Writing output figure to', fig_path)
plt.figure(figsize=(6, 4))
df = pd.read_csv(output_path)
plt.plot(df['fhthreads'], df['tput_gibs'], marker='o', linestyle='-')
plt.xlabel('Application Threads')
plt.ylabel('Throughput (GiB/s)')
plt.title('Sequential Read Throughput vs Num App Threads')
fig_path.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(fig_path)
plt.close()
