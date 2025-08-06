# Reproducing SOSP '25 Paper Evaluations

Evaluation proceeds in multiple phases, as per the SOSP Artifact Guidelines:

- [Data Collection Phase](#data-collection-phase) describes how to run our
  evaluations and collect data. 
- [Data Processing Phase](#data-processing-phase) describes how to process the
  prior data into a usable form (CSVs). 
- [Data Presentation Phase](#data-presentation-phase) converts the CSVs into
  Figures for the paper. 


## Data Collection Phase

### Sequential Read Benchmark: Tput + Latency Only

*Estimated Time: 6m per data point*. 

Run `$MIND_ROOT/apps/sequential-read/run-benchmark.zsh`, and data will be
generated in `$MIND_ROOT/apps/sequential-read`. 

Here's what the script does: 

1. Compile the benchmark application. 
2. Ensure kernel is compiled for the right application. 
   (Mage-Linux can run arbitrary applications; but to simplify implementation,
   it will apply its logic only on executables matching a given name; eg
   "memcached"). 
3. Set up the Mage cluster. Run the application. 
4. Reset the Mage cluster. (Mage has a bug when exiting applications, so we
   reset the cluster for stability). 

This benchmark collects throughput (event counts) and latency samples. 

### Sequential Read Benchmark: Latency Breakdown Only

*Estimated Time: 7m per data point*. 

*Please run this benchmark on the `sosp_ae_latency_breakdown` branch only!*
All other benchmarks should be run on the default branch, as usual. 

Why another branch? 
Mage attempts to optimize ~100-nanosecond level delays in the far-memory
datapath. Even the profiling points (which use the `rdtcsp` x86 instruction)
can cause significant overhead; so most evaluations use the minimal number of
profile points. 
To make the review process simpler, we created a new git branch with
additional profile points placed throughout the codebase. No other changes
have been made to Mage-Linux. You can verify this claim by running
`git diff main sosp_ae_latency_breakdown`. 

The benchmarking process is otherwise exactly the same as the prior sequential
read benchmark. Run
`$MIND_ROOT/apps/sequential-read-latency-breakdown/run-benchmark.zsh`, and
data will be generated in `$MIND_ROOT/apps/sequential-read-latency-breakdown`. 


### Page-Rank (GapBS) Benchmark

*Estimated Time: 10m per data point*. 

This application requires a large Kroenecker graph dataset to traverse. 
Please move this graph to `~/kron.sg` in the Compute Node VM. 

**SOSP Evaluators: you can find this file on the VM host, in your home
directory.** We don't include it in the VM image, because there's not much disk
space in the VM. We want you to be able to test the prior steps, without
running into "out of storage" issues.

Once you have copied the file into the VM, run
`$MIND_ROOT/apps/page-rank/run-benchmark.zsh`.
Data will be generated in `$MIND_ROOT/apps/page-rank`. 

### XSBench Benchmark

*Estimated Time: 10m per data point*. 

Run `$MIND_ROOT/apps/xsbench/run-benchmark.zsh`.
Data will be generated in `$MIND_ROOT/apps/xsbench`. 


## Data Processing Phase

Before starting this phase, please ensure that `pandas`, `numpy`, and
`matplotlib` are installed on the VM Host. In addition to these Python 
libraries, the [ansifilter](https://github.com/andre-simon/ansifilter/blob/master/INSTALL)
program is needed to process dmesg results.
We have included a script to install these dependencies in the host environment
for you; SOSP evaluators will find them pre-installed on our test machines. 

### Figure 14a: Sequential Read Throughput

Run `$MIND_ROOT/scripts/evals/fig-14a-seq_read-tput.py`. 
The output will appear in `$MIND_ROOT/scripts/evals/csv/fig-14a-seq_read-tput.csv`. 
A graph will appear in `$MIND_ROOT/scripts/evals/fig/fig-14a-seq_read-tput.csv`. 

### Figure 14b: Sequential Read 99% Latency

Run `$MIND_ROOT/scripts/evals/fig-14a-seq_read-lat.py`. 
The output will appear in `$MIND_ROOT/scripts/evals/csv/fig-14a-seq_read-lat.csv`. 
A graph will appear in `$MIND_ROOT/scripts/evals/fig/fig-14a-seq_read-lat.csv`. 

NOTE: The Mage-Linux profiling points can only store 2048 samples at once. 
This means that the benchmark must be run many times in order to gather enough
samples (a very time consuming process). 
Our helper script only runs the benchmarks once. 
Its results are usually a good approximation of the graph shown in our SOSP paper. 
Patient reviewers are welcome to re-run the script and collect more CSV samples; the results
will converge to what we've shown in our paper. 

### Figure 15: Sequential Read Latency Breakdown

Run `$MIND_ROOT/scripts/evals/fig-15-lat-breakdown.py`. 
The outputs will appear in `$MIND_ROOT/scripts/evals/csv/fig-15-lat-out.$NUM_THREADS-threads.csv`
for 24 and 48 threads. 
The corresponding graphs will appear in
`$MIND_ROOT/scripts/evals/fig/fig-15-lat-out.$NUM_THREADS-threads.png`. 
