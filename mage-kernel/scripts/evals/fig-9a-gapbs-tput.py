#!/usr/bin/env python

import re
import os
import sys
import csv
from pathlib import Path
from typing import Dict
import sqlite3 as sql

import pandas as pd
import matplotlib.pyplot as plt

from logbox import LogBox

# ----------------
# PARAMETERS
# ----------------

csv_path = Path(f'./csv/fig-9a-gapbs-tput.csv')
fig_path = Path(f'./fig/fig-9a-gapbs-tput.png')
data_path = Path()

# ----------------
# Helper Functions
# ----------------

def scrape_gapbs_data(conn: sql.Connection, base_dir: Path) -> int:
    """Scrape GapBS timing from all log dirs under base_dir

    Returns the number of logs scraped."""

    dfs = []
    current_run_uuid = 0

    for log_dir in (x for x in base_dir.rglob('*') if x.is_dir()):  
        match = re.match(r'^cn(\d+)-fh(\d+)-bs(\d+)-lmem_mib(\d+)-logs.(\d+)$', log_dir.name)
        if not match: 
            continue
        cnthreads = int(match.group(1))
        fhthreads = int(match.group(2))
        batch_size = int(match.group(3))
        lmem_mib = int(match.group(4))
        run = int(match.group(5))

        
        for log_file in log_dir.iterdir(): 
            match = re.match(r'(\d+).log', log_file.name)
            if not match: 
                continue
            assert int(match.group(1)) == fhthreads

            # Search log_file for a line containing "Average" and extract the third field
            avg_time = None
            with open(log_file, 'r') as f:
                for line in f:
                    if "Average Time" not in line:
                        continue
                    fields = line.strip().split()
                    assert len(fields) == 3
                    avg_time = float(fields[2])
            
            assert avg_time is not None

            df_avg_time = pd.DataFrame({
                'rid': [current_run_uuid],
                'time': [avg_time],
                'fhthreads': [fhthreads],
                'cnthreads': [cnthreads],
                'batch_size': [batch_size],
                'lmem_mib': [lmem_mib],
                'run': [run], 
            }, index=None)
            dfs.append(df_avg_time)

            current_run_uuid += 1

    if len(dfs) == 0: 
        return 0

    summary_data = pd.concat(dfs, ignore_index=True)

    # write to sqlite database
    summary_data.to_sql('summaries', conn, if_exists='replace', index=False)
    conn.commit()

    return len(dfs)



# ----------------
# Script
# ----------------

# Set up CWD
mind_root = os.environ.get('MIND_ROOT')
if mind_root is None:
    print('Error: $MIND_ROOT not set!', file=sys.stderr)
    sys.exit(1)

log_dir = Path(mind_root) / 'apps' / 'page-rank'
if not log_dir.is_dir():
    print('Error: $MIND_ROOT/apps/page-rank missing!', file=sys.stderr)
    sys.exit(1)

# Scrape Logs!
conn = sql.connect(':memory:')
scrape_gapbs_data(conn, log_dir)

# Query Logs!
df = pd.read_sql(f'''
        select lmem_mib, time from summaries
        where 
            cnthreads = 4 and fhthreads = 48
            and batch_size = 256 and run = 1
        -- everything but CPU 
        order by lmem_mib asc''', conn)

df['tput'] = 3600 / df['time']
max_lmem = max(df['lmem_mib'])
df['lmem_percent'] = (100 * df['lmem_mib'] / max_lmem)

print('Writing output CSV to', csv_path)
csv_path.parent.mkdir(parents=True, exist_ok=True)
df.to_csv(csv_path, index=False, columns=['lmem_percent', 'tput'])

print('Writing output figure to', fig_path)
plt.figure(figsize=(6, 4))
df = pd.read_csv(csv_path)
plt.plot(df['lmem_percent'], df['tput'], marker='o', linestyle='-')
plt.xlabel('Local Memory (%)')
plt.ylabel('Throughput (jobs/hr)')
plt.title('GapBS Page-Rank Throughput vs % Local Memory')
fig_path.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(fig_path)
plt.close()

conn.close()
