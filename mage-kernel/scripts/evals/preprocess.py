#!/usr/bin/env python3

"""Preprocess sample-based PP data into clean CSV files.

This generates CSV files in the same directory as the original log files.
"""

import csv
import sys
import tempfile
from pathlib import Path
import re
import shlex
import subprocess as sp

default_root_dir = '.'
log_file_patterns = { r'^post\.\d+\.log$', r'^during-\d+\.\d+\.log$' }



def _strip_ansi_escape_chars(source, dest):
    quoted_source = shlex.quote(str(source))
    quoted_dest = shlex.quote(str(dest))
    sp.run(f'ansifilter {quoted_source} -o {quoted_dest}', shell=True)


def _extract_pp_summaries(source, dest):
    """Read a dmesg dump at `source`, convert it to a csv at `dest`"""

    assert not Path(dest).exists()
    output_file_empty = True

    with open(source, 'r') as f:
        lines = f.readlines()

    header_regexp = r'^\[[\s\d\.]*\]\s+Kernel Profile Points: Begin Statistics\s*$'
    footer_regexp = r'^\[[\s\d\.]*\]\s+Kernel Profile Points: End Statistics\s*$'

    start, end = 0, 0
    for i in range(len(lines)-1, -1, -1):
        if re.match(footer_regexp, lines[i]):
            end = i
            break
        del lines[i]
    for i in range(end-1, -1, -1):
        if re.match(header_regexp, lines[i]):
            start = i + 1
            break

    row_regexp = re.compile(r'''^\[[\s\d\.]*\]\s+
                            pp:\s+(\w+),\s+
                            cpu:\s+(\d+),\s+
                            time_sec:\s+([\d\.]+),\s+
                            nr:\s+(\d+)\s*$''', re.VERBOSE)
    with open(dest, 'w', newline='') as f:
        writer = csv.DictWriter(f, ['pp_name', 'cpu', 'total(s)', 'nr'],
                                dialect=csv.unix_dialect,
                                quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        for i in range(start, end):
            match = row_regexp.match(lines[i])
            if not match:
                continue
            output_file_empty = False
            writer.writerow({
                'pp_name': match.group(1),
                'cpu': int(match.group(2)),
                'total(s)': float(match.group(3)),
                'nr': int(match.group(4)),
            })

    if output_file_empty:
        print(f"Warning: empty output file: {dest}")


def _extract_pp_samples(source, dest):
    """Read a dmesg dump at `source`, convert it to a csv at `dest`"""

    assert not Path(dest).exists()
    output_file_empty = True

    with open(source, 'r') as f:
        lines = f.readlines()

    start, end = None, None
    min_intersample_delay, max_intersample_delay = -1, -1

    for i in range(len(lines)-1, -1, -1):
        if re.match(r'^\[.*\] Kernel Profile Points: End Sampled Latencies.*$', lines[i]):
            end = i
            break
        del lines[i]
    if end is None:
        print(f"Warning: input file with missing sample end: {source}")
        return

    begin_regex = re.compile(
            r'''
             ^\[.*\]\ Kernel\ Profile\ Points:\s+
             Begin\ Sampled\ Latencies:\s*
             \(min_delay=(\d+)\ max_delay=(\d+)\)\s*$
             ''',
             re.VERBOSE)

    for i in range(end-1, -1, -1):
        match = begin_regex.match(lines[i])
        if match:
            start = i
            min_intersample_delay = int(match.group(1))
            max_intersample_delay = int(match.group(2))
            break
    if start is None:
        print(f"Warning: input file with missing sample start: {source}")
        return

    f = open(dest, 'w', newline='')
    writer = csv.DictWriter(f, ['pp_name', 'cpu', 'sample(ns)',
                                'min_intersample_delay',
                                'max_intersample_delay'],
                            dialect=csv.unix_dialect,
                            quoting=csv.QUOTE_MINIMAL)
    writer.writeheader()

    pp_begin_regexp = r'^\[[\s\d\.]*\]\s+Begin Sampled Latencies\((\w+)\):\s+$'
    pp_end_regexp   = r'^\[[\s\d\.]*\]\s+End Sampled Latencies\((\w+)\):\s+$'
    cpu_begin_regexp = r'^\[[\s\d\.]*\]\s+Sampled Latencies from CPU (\d+):.*$'

    cpu = None
    pp_name = None
    for i in range(start, end):
        # Get Sample latencies for one PP
        match = re.match(pp_begin_regexp, lines[i])
        if match:
            pp_name = str(match.group(1))
            continue

        # Skip "end sample" latencies for one PP.
        match = re.match(pp_end_regexp, lines[i])
        if match:
            pp_name = None
            continue

        # Get sample latencies for each CPU (per-CPU var, remember?).
        match = re.match(cpu_begin_regexp, lines[i])
        if match:
            cpu = int(match.group(1))
            continue

        match = re.match(r'^\[[\s\d\.]*\]([\s\d]+)$', lines[i])
        if not match:
            continue
        samples = match.group(1).strip()
        samples = re.findall(r'\b\d+\b', samples)

        for sample in samples:
            writer.writerow({
                'pp_name': pp_name,
                'cpu': cpu,
                'sample(ns)': int(sample),
                'min_intersample_delay': min_intersample_delay,
                'max_intersample_delay': max_intersample_delay,
            })
            output_file_empty = False

    if output_file_empty:
        print(f"Warning: empty output file: {dest}")

    f.close()


def _is_logfile(path):
    if path.is_dir():
        return False
    for pattern in log_file_patterns:
        if re.match(pattern, path.name):
            return True
    return False

def process_dmesg_logs(root_dir: Path | str) -> int:
    num_processed = 0
    root_dir = Path(root_dir)

    for logfile in root_dir.rglob('*'):
        if not _is_logfile(logfile):
            continue

        summary_file = logfile.with_name(f"pp_summary.{logfile.stem}.csv")
        samples_file = logfile.with_name(f"pp_samples.{logfile.stem}.csv")

        if summary_file.exists() and samples_file.exists():
            continue

        text_fd = tempfile.NamedTemporaryFile(delete=False)
        text_file = Path(text_fd.name)
        _strip_ansi_escape_chars(logfile, text_file)

        if not summary_file.exists():
            _extract_pp_summaries(text_file, summary_file)
        if not samples_file.exists():
            _extract_pp_samples(text_file, samples_file)

        text_fd.close()
        text_file.unlink()

        num_processed += 1

    return num_processed


def _main():
    num_processed = 0
    args = sys.argv[1:]
    if (len(args) == 0):
        print(f'Processing files in `{default_root_dir}`')
        num_processed += process_dmesg_logs(default_root_dir)

    for arg in args:
        num_processed += process_dmesg_logs(arg)

    print(f'{num_processed} logfiles processed.')

if __name__ == '__main__':
    _main()
