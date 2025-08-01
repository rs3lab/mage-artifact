#ifndef MODULE_NAME
#define MODULE_NAME "fbs_profile_sampler"
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_ALERT */
#include "../../include/disagg/profile_points_disagg.h"

MODULE_LICENSE("GPL");

static int sample;
module_param(sample, int, 0000);  // Module parameter for IP address
MODULE_PARM_DESC(sample, "1 => activate sampling. 0 => deactivate.");

static int __init profile_sampler_init(void)
{
	pr_info("Profile sampler inserted...setting sampling mode to %d\n",
			sample);
	set_pps_sample_mode(sample);
	return 0;
}

static void __exit profile_sampler_exit(void)
{
	;
}

/* module init and exit */
module_init(profile_sampler_init)
module_exit(profile_sampler_exit)
