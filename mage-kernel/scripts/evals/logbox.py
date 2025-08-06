#!/usr/bin/env python3

import math
import re
import sqlite3 as sql
from collections import defaultdict
from pathlib import Path

import numpy as np
import pandas as pd

from preprocess import process_dmesg_logs


class Scraper():
    """Parses raw Mage-Linux logs into its internal SQL store."""

    conn: sql.Connection

    def __init__(self) -> None:
        self.conn = sql.connect(':memory:')
        self.current_run_uuid = 0

    def __del__(self):
        self.conn.close()

    def scrape_log_files(self, base_dir: Path) -> int:
        """Load PP data and HW Data into internal SQL database"""

        process_dmesg_logs(base_dir)

        num_scraped = self._scrape_pp_data(base_dir)
        if num_scraped == 0:
            print('No files detected to scrape!')
            return 0
        self._scrape_hw_data(base_dir)
        return num_scraped

    def _scrape_pp_data(self, base_dir: Path) -> int:
        """Scrape PP data from all log dirs under base_dir

        Returns the number of dataframes scraped."""

        summary_dfs = []
        sample_dfs = []

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
                match = re.match(r'post.(\d+).log', log_file.name)
                if not match: 
                    continue
                assert int(match.group(1)) == fhthreads

                summary_file = log_file.with_name(f"pp_summary.{log_file.stem}.csv")
                sample_file = log_file.with_name(f"pp_samples.{log_file.stem}.csv")
                if not summary_file.exists(): 
                    raise Exception(f"Missing summary file {summary_file}!")
                if not sample_file.exists(): 
                    raise Exception(f"Missing samples file {sample_file}!")

                # Read in the PP Summary Data
                df = pd.read_csv(summary_file, index_col=None)
                df['rid'] = self.current_run_uuid
                df['fhthreads'] = fhthreads
                df['cnthreads'] = cnthreads
                df['batch_size'] = batch_size
                df['lmem_mib'] = lmem_mib
                df['run'] = run
                summary_dfs.append(df)

                # Read in the PP Sample Data
                df = pd.read_csv(sample_file, index_col=None)
                df['rid'] = self.current_run_uuid
                sample_dfs.append(df)

                self.current_run_uuid += 1

        if len(summary_dfs) == 0 or len(sample_dfs) == 0:
            return 0

        summary_data = pd.concat(summary_dfs, ignore_index=True)
        sample_data = pd.concat(sample_dfs, ignore_index=True)

        # write to sqlite database
        summary_data.to_sql('summaries', self.conn, if_exists='replace', index=False)
        sample_data.to_sql('samples', self.conn, if_exists='replace', index=False)
        self.conn.commit()

        return len(summary_dfs)

    def _scrape_hw_data(self, base_dir: Path) -> None:
        """Scrape HW NIC counter data from all log dirs under base_dir"""

        data = []
        for log_dir in (x for x in base_dir.rglob('*') if x.is_dir()):  
            match = re.match(r'^cn(\d+)-bs(\d+)-lmem_mib(\d+)-logs.(\d+)$', log_dir.name)
            if not match: 
                continue
            cnthreads = int(match.group(1))
            batch_size = int(match.group(2))
            lmem_mib = int(match.group(4))
            run = int(match.group(3))
            
            for hwpre_file in log_dir.glob('*'):  
                match = re.match(r'pre.hwcounter.(\d+).log', hwpre_file.name)
                if not match: 
                    continue
                    
                fhthreads = int(match.group(1))
            
                hwpost_file = Path(str(hwpre_file).replace('pre.hwcounter', 'post.hwcounter'))
                if not hwpost_file.exists(): 
                    raise Exception(f"Missing hwpost {hwpost_file}")

                # Read the file contents to get tx and rx values
                txpre, rxpre = None, None
                with open(hwpre_file, 'r') as f:
                    for line in f.readlines(): 
                        if (match := re.match(r'\s+tx_bytes_phy:\s+(\d+)$', line)): 
                            txpre = int(match.group(1))
                        if (match := re.match(r'\s+rx_bytes_phy:\s+(\d+)$', line)): 
                            rxpre = int(match.group(1))
                assert txpre is not None and rxpre is not None

                # Read the file contents to get tx and rx values
                txpost, rxpost = None, None
                with open(hwpost_file, 'r') as f:
                    for line in f.readlines(): 
                        if (match := re.match(r'^\s*tx_bytes_phy:\s*(\d+)\s*$', line)): 
                            txpost = int(match.group(1))
                        if (match := re.match(r'^\s*rx_bytes_phy:\s*(\d+)$\s*', line)): 
                            rxpost = int(match.group(1))
                assert txpost is not None and rxpost is not None
            
                tx = txpost - txpre
                rx = rxpost - rxpre
                
                # Append the extracted data to the list
                data.append([fhthreads, cnthreads, batch_size, lmem_mib, tx, rx, run])

        # Convert the list to a DataFrame
        columns = ['fhthreads', 'cnthreads', 'batch_size', 'lmem_mib', 'tx', 'rx', 'run']
        df = pd.DataFrame(data, columns=columns)

        # Write the data to a sqlite3 database.
        df.to_sql('hwcount', self.conn, if_exists='replace', index=False)
        self.conn.commit()



class LogBox():
    """Stores and Queries Mage-Linux data."""

    def __init__(self):
        # scrapes new log files
        self.scraper = Scraper()
        self.conn = self.scraper.conn

    def scrape_log_files(self, base_dir: Path) -> int:
        """Returns the number of log files scraped."""
        return self.scraper.scrape_log_files(base_dir)

    def get(self, query: str):
        return pd.read_sql(query, self.conn)
