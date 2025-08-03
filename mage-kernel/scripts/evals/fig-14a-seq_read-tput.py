#!/usr/bin/env python

import os
import sys
from pathlib import Path
import pandas as pd

from logbox import LogBox

# ----------------
# PARAMETERS
# ----------------

bench_time = 30
page_size = 4 * 1024
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
df.to_csv(output_path, index=False)
