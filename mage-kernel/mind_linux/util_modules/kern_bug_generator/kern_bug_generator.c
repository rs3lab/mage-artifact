#ifndef MODULE_NAME
#define MODULE_NAME "kern_bug_generator"
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_ALERT */
#include "../../include/disagg/profile_points_disagg.h"

MODULE_LICENSE("GPL");

static int __init kern_bug_gen_init(void)
{
	pr_info("Kernel bug generator inserted... Calling BUG()...\n");
    BUG();
	return 0;
}

static void __exit kern_bug_gen_exit(void)
{
	;
}

/* module init and exit */
module_init(kern_bug_gen_init)
module_exit(kern_bug_gen_exit)
