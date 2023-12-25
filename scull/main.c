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
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include "scull.h"


int scull_major =   SCULL_MAJOR;
int scull_minor =   0;
int scull_nr_devs = SCULL_NR_DEVS;	/* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;
int scull_qset =    SCULL_QSET;

// S_IRUGO: parameter that is read only by the world
// S_IRUGO | S_IWUSR: parameter can only be modified by the root user
module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

MODULE_AUTHOR("Gaby, Kim");
MODULE_DESCRIPTION("scull device driver in Linux Device Driver with linux 6.7.0");
MODULE_LICENSE("GPL v2");

struct scull_dev *scull_devices;	/* allocated in scull_init_module */

/*
 * Empty out the scull device; must be called with the device
 * semaphore held.
 */
int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *curr, *next; 
	int qset = dev->qset;

	// every kmalloced pointer should be kfreed from inside out
	for (curr = dev->data; curr; curr = next) {
		if (curr->data) {
			for (int i = 0; i < qset; ++i)
				kfree(curr->data[i]);
			kfree(curr->data);
			curr->data = NULL;
		}
		next = curr->next;
		kfree(curr);
	}
	
	// reset all the information of scull_dev
	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;

	return 0;
}

/*
 * Open and close
 */
int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;

	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		printk(KERN_DEBUG "scull device is opened with O_WRONLY flag\n");
		scull_trim(dev);
		up(&dev->sem);
	}
	printk(KERN_DEBUG "scull device is opened\n");

	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_DEBUG "scull device is released\n");
	return 0;
}

// scull device lazily initialize
struct scull_qset* scull_follow(struct scull_dev *dev, int n)
{
	// initialize the list head if needed
	if (!dev->data) {
		dev->data = kzalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (!dev->data)
			return NULL;
	}

	struct scull_qset *target = dev->data;
	while (n--) {
		if (!target->next) {
			target->next = kzalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (!target->next)
				return NULL;
		}
		target = target->next;
	}
	return target;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	struct scull_dev *dev = filp->private_data;

	if (down_interruptible(&dev->sem)) {
		retval = -ERESTARTSYS;
		return retval;
	}

	// case 1. offset is alreay beyond EOF
	if (*f_pos >= dev->size)
		goto done;
	
	// case 2. offset is within EOF, while offset + count is beyond EOF
	if (*f_pos + count > dev->size)
		count = dev->size - *f_pos;

	// case 3. offset + count is within EOF, look for the target quantum to copy to user space
	int list_node_size = dev->qset * dev->quantum;
	// locate the q_set list node
	int qset_pointer_offset = *f_pos / list_node_size;
	struct scull_qset *target = scull_follow(dev, qset_pointer_offset);

	// then the quantum pointer, finally pinpoint the f_pos within the quantum
	int quantum_pointer_offset = (*f_pos % list_node_size) / dev->quantum;
	int quantum_offset = (*f_pos % list_node_size) % dev->quantum;
	
	if (!target || !target->data || !target->data[quantum_pointer_offset])
		goto done;

	// read only up to the end of this quantum
	count = min(count, (size_t)dev->quantum - quantum_offset);
	if (copy_to_user(buf, target->data[quantum_pointer_offset] + quantum_offset, count)) {
		retval = -EFAULT;
		goto done;
	}
	*f_pos += count;
	retval = count;

done:
	up(&dev->sem);
	printk(KERN_INFO "read %ld bytes from scull device", retval);

	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	struct scull_dev *dev = filp->private_data;

	if (down_interruptible(&dev->sem))
		return ERESTARTSYS;

	
	int list_node_size = dev->qset * dev->quantum;
	// locate the q_set list node
	int qset_pointer_offset = *f_pos / list_node_size;
	struct scull_qset *target = scull_follow(dev, qset_pointer_offset);
	if (!target)
		goto done;

	// then the quantum pointer, finally pinpoint the f_pos within the quantum
	int quantum_pointer_offset = (*f_pos % list_node_size) / dev->quantum;
	int quantum_offset = (*f_pos % list_node_size) % dev->quantum;

	// allocate quantum set if needed
	if (!target->data) {
		target->data = kzalloc(dev->qset * sizeof(void *), GFP_KERNEL);
		if (!target->data)
			goto done;
	}

	// allocate quantum if needed
	if (!target->data[quantum_pointer_offset]) {
		target->data[quantum_pointer_offset] = kzalloc(dev->quantum, GFP_KERNEL);
		if (!target->data[quantum_pointer_offset])
			goto done;
	}

	// write only up to this quantum
	count = min(count, (size_t)dev->quantum - quantum_offset);
	if (copy_from_user(target->data[quantum_pointer_offset] + quantum_offset, buf, count)) {
		retval = -EFAULT;
		goto done;
	}

	*f_pos += count;
	retval = count;

	// update size of the device
	dev->size = max((unsigned long)*f_pos, dev->size);

done:
	up(&dev->sem);
	printk(KERN_INFO "write %ld bytes to scull device", retval);

	return retval;
}

struct file_operations scull_fops = {
	.owner =    	THIS_MODULE,
	.open = 		scull_open,
	.release = 		scull_release,
	.read = 		scull_read,
	.write = 		scull_write,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* Get rid of our char dev entries */
	if (scull_devices) {
		for (int i = 0; i < scull_nr_devs; i++) {
			scull_trim(&scull_devices[i]);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, scull_nr_devs);
}

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int devno = MKDEV(scull_major, scull_minor + index);
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	int err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		pr_warn("Error %d addding scull%d\n", err, index);
}

static int __init scull_init_module(void)
{
	int result;
	dev_t dev = 0;

	/*
	* Get a range of minor numbers to work with, asking for a dynamic
	* major unless directed otherwise at load time.
	*/
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs,
				"scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		pr_warn("scull: can't get major %d\n", scull_major);
		return result;
	}

	/* 
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	scull_devices = kzalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}

	for (int i = 0; i < scull_nr_devs; ++i) {
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		sema_init(&scull_devices[i].sem, 1);
		scull_setup_cdev(&scull_devices[i], i);
	}

	return 0;

fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
