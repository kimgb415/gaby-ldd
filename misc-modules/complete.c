#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>  /* current and everything */
#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */

#include <linux/completion.h>

#include "misc.h"

MODULE_LICENSE("GPL v2");

static int complete_major = 0;

DECLARE_COMPLETION(comp);

ssize_t complete_read(struct file *filp, char __user *buf, size_t count, loff_t* offset)
{
    // Alawys signal EOF after the first read
    if (*offset)
        return 0;

    MDEBUG("Process %i (%s) going to sleep\n", current->pid, current->comm);
    if (wait_for_completion_interruptible(&comp))
        return -ERESTARTSYS;

    MDEBUG("Awoken process %is (%s)\n", current->pid, current->comm);

    char temp[50] = "Writer finally worte something\n";
    if (copy_to_user(buf, temp, sizeof(temp)))
        return -EFAULT;

    *offset += sizeof(temp);

    return sizeof(temp);
}

ssize_t complete_write(struct file *filp, const char __user *buf, size_t count, loff_t* offset)
{
    MDEBUG("Process %i %s awakening the readers...\n", current->pid, current->comm);
    complete(&comp);

    return count;
}

struct file_operations complete_ops = {
    .owner = THIS_MODULE,
    .read = complete_read,
    .write = complete_write,
};

int complete_init(void)
{
	int result;

	/*
	 * Register your major, and accept a dynamic number
	 */
	result = register_chrdev(complete_major, "complete", &complete_ops);
	if (result < 0)
		return result;
	if (complete_major == 0)
		complete_major = result; /* dynamic */
    MDEBUG("Complete module initialized\n");
    
	return 0;
}

void complete_cleanup(void)
{
	unregister_chrdev(complete_major, "complete");
    MDEBUG("Complete module is cleaned up\n");
}

module_init(complete_init);
module_exit(complete_cleanup);