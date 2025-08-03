#!/usr/bin/env python

import os
import sys
import csv
from pathlib import Path

from logbox import LogBox

# ----------------
# PARAMETERS
# ----------------

output_path = './out.csv'

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

with open(output_path, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(('fhthreads', '99_latency'))
    writer.writerows(out)
