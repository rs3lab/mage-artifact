#ifndef MODULE_NAME
#define MODULE_NAME "fbs_profile_cleaner"
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_ALERT */
#include "../../include/disagg/profile_points_disagg.h"

MODULE_LICENSE("GPL");

static int __init profile_cleaner_init(void)
{
	pr_info("Profile cleaner inserted... Let's clear profile result (for now)\n");
	clear_pps();
	return 0;
}

static void __exit profile_cleaner_exit(void)
{
	;
}

/* module init and exit */
module_init(profile_cleaner_init)
module_exit(profile_cleaner_exit)
