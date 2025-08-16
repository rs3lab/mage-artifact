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

csv_path = Path(f'./csv/fig-12-metis-tput.csv')
map_fig_path = Path(f'./fig/fig-12a-metis_map-tput.png')
reduce_fig_path = Path(f'./fig/fig-12b-metis_reduce-tput.png')

# ----------------
# Helper Functions
# ----------------

def scrape_metis_data(conn: sql.Connection, base_dir: Path) -> int:
    """Scrape Metis timing from all log dirs under base_dir

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
            map_time_ms, reduce_time_ms = None, None
            with open(log_file, 'r') as f:
                for line in f:
                    if "Real" not in line:
                        continue
                    fields = line.strip().split()

                    map_time_ms = int(fields[5])
                    reduce_time_ms = int(fields[9])
            
            assert map_time_ms is not None and reduce_time_ms is not None

            dfs.append(pd.DataFrame({
                'rid': [current_run_uuid],
                'map_time_ms': [map_time_ms],
                'reduce_time_ms': [reduce_time_ms],
                'fhthreads': [fhthreads],
                'cnthreads': [cnthreads],
                'batch_size': [batch_size],
                'lmem_mib': [lmem_mib],
                'run': [run], 
            }, index=None))

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

log_dir = Path(mind_root) / 'apps' / 'metis-mapreduce'
if not log_dir.is_dir():
    print('Error: $MIND_ROOT/apps/metis-mapreduce missing!', file=sys.stderr)
    sys.exit(1)

# Scrape Logs!
conn = sql.connect(':memory:')
scrape_metis_data(conn, log_dir)

# Query Logs, Process Columns!
df = pd.read_sql(f'''
        select lmem_mib, map_time_ms, reduce_time_ms from summaries
        where 
            cnthreads = 4 and fhthreads = 48
            and batch_size = 256 and run = 1
        -- everything but CPU 
        order by lmem_mib asc''', conn)

df['map_tput'] = 3600* 1e3 / df['map_time_ms']
df['reduce_tput'] = 3600* 1e3 / df['reduce_time_ms']
max_lmem = max(df['lmem_mib'])
df['lmem_percent'] = (100 * df['lmem_mib'] / max_lmem)


# Save Output

print('Writing output CSV to', csv_path)
csv_path.parent.mkdir(parents=True, exist_ok=True)
df.to_csv(csv_path, index=False, columns=['lmem_percent', 'map_tput', 'reduce_tput'])

print('Writing output figures to', map_fig_path, reduce_fig_path)

# map figure
plt.figure(figsize=(6, 4))
df = pd.read_csv(csv_path)
plt.plot(df['lmem_percent'], df['map_tput'], marker='o', linestyle='-')
plt.xlabel('Local Memory (%)')
plt.ylabel('Throughput (jobs/hr)')
plt.title('Metis Map-Phase Throughput vs % Local Memory')
map_fig_path.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(map_fig_path)
plt.close()

# reduce figure
plt.figure(figsize=(6, 4))
plt.plot(df['lmem_percent'], df['reduce_tput'], marker='o', linestyle='-')
plt.xlabel('Local Memory (%)')
plt.ylabel('Throughput (jobs/hr)')
plt.title('Metis Reduce-Phase Throughput vs % Local Memory')
reduce_fig_path.parent.mkdir(parents=True, exist_ok=True)
plt.savefig(reduce_fig_path)
plt.close()

conn.close()
