#!/usr/bin/env python

import os
import sys
import csv
from pathlib import Path

from logbox import LogBox
from typing import Dict

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

def over_fh(metric, pp, fh):
    return logs.get(f'''
        select sum(`{metric}`) as out from summaries
        where 
            pp_name = '{pp}'
            and cnthreads = 4
            -- and fhthreads = {fh}
            and batch_size = 256
            and run = 1
        -- everything but CPU 
        group by pp_name, rid, fhthreads, cnthreads, batch_size, run
        order by cnthreads, fhthreads, batch_size asc''')['out']

def us_per_fault(pp, fh):
    return 1e6 * over_fh('total(s)', pp, fh) / over_fh('nr', 'FH_total', fh)

def get_breakdown(fh) -> Dict[str,float]:
    upf = lambda pp: us_per_fault(pp, fh)
    categories = {
            'RDMA': upf('FH_rdma'),
            'Page Circulation': upf('FH_init_vma') + upf('FH_cleanup'),
            'LRU Update': upf('FH_rangelock')
    }
    unaccounted_time = upf('FH_total')
    for category in categories.values():
        unaccounted_time -= category
    categories['other'] = unaccounted_time

    return categories

def to_output_csv(fh, path):
    with open(path, mode='w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(('category', 'latency'))
        for c, l in get_breakdown(fh):
            writer.writerow((c, l))

for fh in (24, 48):
    to_output_csv(fh, f'./fig-15-lat-out.fh-threads.csv')
