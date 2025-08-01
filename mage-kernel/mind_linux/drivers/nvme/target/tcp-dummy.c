/*
 * NVMe over Fabrics RDMA target.
 * Copyright (c) 2015-2016 HGST, a Western Digital Company.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/atomic.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/nvme.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/inet.h>
#include <asm/unaligned.h>

#include "nvmet.h"

static int __init nvmet_tcp_init(void)
{
	// Dummy module
	printk(KERN_WARNING "You are using dummy module\n");
	return 0;
}

static void __exit nvmet_tcp_exit(void)
{
	;
}

module_init(nvmet_tcp_init);
module_exit(nvmet_tcp_exit);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("nvmet-transport-3"); /* 3 == NVMF_TRTYPE_TCP */
