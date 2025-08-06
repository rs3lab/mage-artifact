#!/usr/bin/env python

import os
import sys
import csv
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt

from logbox import LogBox

# ----------------
# PARAMETERS
# ----------------

output_path = Path('./csv/fig-14a-seq_read-lat.csv')
fig_path = Path('./fig/fig-14a-seq_read-lat.png')

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

logs.scrape_log_files(log_dir)


# Query Logs!

def samples(fh): 
    return logs.get(f"""
        select `sample(ns)`
        from samples
        inner join summaries on
            samples.rid = summaries.rid and summaries.pp_name = samples.pp_name
        where 
            samples.pp_name = 'FH_total'
            and batch_size = 256
            and run = 1
            and fhthreads = {fh}
            and cnthreads = 4
    """)['sample(ns)'] / 1e3 # to microseconds


out = []
for fh in [1, 2, 4, 8, 16, 32, 40, 48]: 
    df = samples(fh)
    out.append((fh, df.quantile(0.99)))

# write output to CSV
print('Writing output CSV to', output_path)
with open(output_path, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(('fhthreads', '99_latency'))
    writer.writerows(out)

print('Writing output figure to', fig_path)
plt.figure(figsize=(6, 4))
df = pd.read_csv(output_path)
plt.plot(df['fhthreads'], df['99_latency'], marker='o', linestyle='-')
plt.xlabel('Application Threads')
plt.ylabel('99% Latency (us)')
plt.title('Sequential Read: 99% Latency vs Num App Threads')
fig_path.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(fig_path)
plt.close()
