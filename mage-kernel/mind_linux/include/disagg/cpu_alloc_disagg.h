#ifndef __CPU_ALLOC_DISAGG_H__
#define __CPU_ALLOC_DISAGG_H__

#include <linux/sched.h>   // struct range_lock

void init_disagg_core_pinning(int max_threads_in_use);
int disagg_pin_fhthread_to_core(struct task_struct *t);
int disagg_pin_cnthread_to_core(struct task_struct *t);
void disagg_print_core_assignments(void);

#endif  /* __CPU_ALLOC_DISAGG_H__ */
