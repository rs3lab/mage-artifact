#ifndef __TNA_DISAGG_SWITCH_BFRT_H__
#define __TNA_DISAGG_SWITCH_BFRT_H__

#include <getopt.h>
#include <unistd.h>
#include <stdio.h>

#ifndef _mem_barrier_
#define _mem_barrier_() asm volatile("" ::: "memory");
#endif

// extern "C" void print_bfrt_addr_trans_rule_counters(void);

void set_datetime_filename(void);
FILE *open_datetime_file(void);
FILE *get_datetime_filep(void);
void close_datetime_file(void);

#endif
