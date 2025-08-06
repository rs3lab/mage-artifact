#!/usr/bin/env python

import os
import sys
import csv
from pathlib import Path
from typing import Dict

import pandas as pd
import matplotlib.pyplot as plt

from logbox import LogBox

# ----------------
# SCRIPT
# ----------------

# Scrape Logs!

logs = LogBox()

mind_root = os.environ.get('MIND_ROOT')
if mind_root is None:
    print('Error: $MIND_ROOT not set!', file=sys.stderr)
    sys.exit(1)

log_dir = Path(mind_root) / 'apps' / 'sequential-read-lat-breakdown'
if not log_dir.is_dir():
    print('Error: $MIND_ROOT/apps/sequential-read-lat-breakdown missing!', file=sys.stderr)
    sys.exit(1)

logs.scrape_log_files(log_dir)

# Query Logs!

def over_fh(metric, pp, fh):
    return logs.get(f'''
        select sum(`{metric}`) as out from summaries
        where 
            pp_name = '{pp}'
            and cnthreads = 4
            and fhthreads = {fh}
            and batch_size = 256
            and run = 1
        -- everything but CPU 
        group by pp_name, rid, fhthreads, cnthreads, batch_size, run
        order by cnthreads, fhthreads, batch_size asc''')['out']

def us_per_fault(pp, fh):
    return 1e6 * over_fh('total(s)', pp, fh) / over_fh('nr', 'FH_total', fh)

def get_breakdown(fh) -> Dict[str,float]:
    assert len(us_per_fault('FH_total', fh)) == 1
    upf = lambda pp: us_per_fault(pp, fh).item()
    categories = {
            'RDMA': upf('FH_rdma'), 
            'Page Circulation': upf('FH_init_vma') + upf('FH_cleanup'), 
            'LRU Update': upf('FH_rangelock'), 
    }
    unaccounted_time = upf('FH_total')
    for category in categories.values():
        unaccounted_time -= category
    categories['other'] = unaccounted_time

    return categories

def to_csv(fh, path: Path):
    print('Writing output CSV to', path)
    path.parent.mkdir(parents=True, exist_ok=True)

    with open(path, mode='w') as f:
        writer = csv.writer(f)
        writer.writerow(('category', 'latency'))
        for c, l in get_breakdown(fh).items():
            writer.writerow((c, l))

def to_graph(fh, csv_path, img_path): 
    print('Writing output figure to', img_path)
    img_path.parent.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(5, 5))
    df = pd.read_csv(csv_path)

    ax.pie(
        df['latency'],
        labels=df['category'],
        autopct='%1.1f%%',
        startangle=90
    )
    ax.set_title(f'Latency Breakdown ({fh} FH Threads)')
    plt.tight_layout()
    plt.savefig(img_path)
    plt.close(fig)


for fh in (24, 48):
    csv_path = Path(f'./csv/fig-15-lat-out.{fh}-threads.csv')
    img_path = Path(f'./fig/fig-15-lat-out.{fh}-threads.png')
    to_csv(fh, csv_path)
    to_graph(fh, csv_path, img_path)
