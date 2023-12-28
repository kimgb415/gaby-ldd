/*
 * Gaby Kim -- Linux Device Driver with linux 6.7.0
 *
 * Original Code from "main.c -- the bare scull char module"
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * This file includes modifications by Gaby, Kim
 * on 12/23/2023
 *
 * The source code in this file is based on code from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published by O'Reilly & Associates.
 * The original code can be freely used, adapted, and redistributed in source or binary form,
 * so long as an acknowledgment appears in derived source files.
 *
 * No warranty is attached to the original code and this modified version; 
 * neither the original authors nor [Your Name or Your Organization] can take 
 * responsibility for errors or fitness for use.
 *
 */


#include <linux/init.h>
#include <linux/module.h>

MODULE_AUTHOR("Gaby, Kim");
MODULE_DESCRIPTION("scull device driver in Linux Device Driver with linux 6.7.0");
MODULE_LICENSE("GPL v2");


static int __init scull_init_module(void)
{
	pr_debug("scull module loaded\n");

	return 0;
}

// cleanup function can't be marked as __exit, if it's used else where other than module_exit
static void __exit scull_cleanup_module(void)
{
	pr_debug("scull module unloaded\n");
};

module_init(scull_init_module);
module_exit(scull_cleanup_module);