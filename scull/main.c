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

struct scull_qset* scull_follow(struct scull_dev*, int);
ssize_t scull_read_util(struct scull_dev*, char __user*, size_t, loff_t*);

#ifdef SCULL_DEBUG /* use proc only if debugging */

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#define SCULL_PROC "scullproc"
#define SCULL_SEQ_PROC "scullseq"

struct proc_dir_entry *scull_proc_entry = NULL;
struct proc_dir_entry *scull_seq_proc_entry = NULL;


/*
 * The proc filesystem: function to read and entry
 */
static ssize_t scull_read_procmem(struct file *filp, char __user *buf, size_t count,  loff_t *offset)
{
	PDEBUG("called with count: %lu, offset: %lld\n", count, *offset);
	loff_t device_offset = *offset;
	ssize_t len = 0;
	for (int i = 0 ; i < scull_nr_devs && len <= count; ++i) {
		struct scull_dev *device = &scull_devices[i];
		if (down_interruptible(&device->sem))
			return -ERESTARTSYS;

		// skip to next device if needed
		if (device_offset >= device->size) {
			device_offset -= device->size;
			up(&device->sem);
			continue;
		}

		len = scull_read_util(device, buf, count, &device_offset);
		// scull_read_util might return -EFAULT
		if (len >= 0) {
			*offset += len;
			PDEBUG("read %ld bytes in total", len);
		}
		up(&device->sem);
		break;
	}

	return len;
}

static const struct proc_ops scull_proc_fops = {
	.proc_read = scull_read_procmem,
};

static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= scull_nr_devs)
		return NULL;
	// seq_printf(s, "iterate starts at /device/scull%lld\n", *pos);
	return scull_devices + *pos;
}

static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	if (++(*pos) >= scull_nr_devs)
		return NULL;
	// seq_printf(s, "iterate to /device/scull%lld\n", *pos);
	return scull_devices + *pos;
}

static int scull_seq_show(struct seq_file *s, void *v)
{
	struct scull_dev *device = v;
	if (down_interruptible(&device->sem))
		return -ERESTARTSYS;

	struct scull_qset *qset = device->data;
	for (; qset; qset = qset->next) {
		if (!qset->data)
			continue;
		for (int j = 0; j < device->qset; ++j) {
			if (!qset->data[j])
				continue;
			seq_printf(s, (const char *)qset->data[j]);
		}
	}

	up(&device->sem);
	return 0;
}

static void scull_seq_stop(struct seq_file *s, void *v)
{
	/* Actually, there's nothing to do here */
}

static const struct seq_operations scull_seq_ops = {
	.start = scull_seq_start,
	.stop = scull_seq_stop,
	.next = scull_seq_next,
	.show = scull_seq_show,
};

static int scull_proc_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &scull_seq_ops);
}

static const struct proc_ops scull_seq_proc_ops = {
	.proc_read = seq_read,
	.proc_open = scull_proc_open,
	.proc_release = seq_release,
	.proc_lseek = seq_lseek,
};

static void scull_create_proc(void)
{
	scull_proc_entry = proc_create(SCULL_PROC, 0644, NULL, &scull_proc_fops);
	if (!scull_proc_entry)
		PDEBUG("/proc/%s not created", SCULL_PROC);

	scull_seq_proc_entry = proc_create(SCULL_SEQ_PROC, 0644, NULL, &scull_seq_proc_ops);
	if (!scull_seq_proc_entry)
		PDEBUG("/proc/%s not created", SCULL_SEQ_PROC);
}

static void scull_remove_proc(void)
{
	proc_remove(scull_proc_entry);
	proc_remove(scull_seq_proc_entry);
}
#endif

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
		PDEBUG( "device file opened with O_WRONLY\n");
		scull_trim(dev);
		up(&dev->sem);
	}
	PDEBUG( "scull device is opened\n");

	return 0;
}

int scull_release(struct inode *inode, struct file *filp)
{
	PDEBUG( "scull device is released\n");
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

ssize_t scull_read_util(struct scull_dev* dev, char __user *buf, size_t count, loff_t *f_pos)
{
	PDEBUG("called with count: %lu, f_pos: %lld\n", count, *f_pos);
	// case 1. offset is alreay beyond EOF
	if (*f_pos >= dev->size)
		return 0;
	
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
		return 0;

	// read only up to the end of this quantum
	count = min(count, (size_t)dev->quantum - quantum_offset);
	if (copy_to_user(buf, target->data[quantum_pointer_offset] + quantum_offset, count))
		return -EFAULT;

	*f_pos += count;
	PDEBUG("read %lu bytes\n", count);

	return count;
}


ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t retval = 0;
	struct scull_dev *dev = filp->private_data;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	retval = scull_read_util(dev, buf, count, f_pos);

	up(&dev->sem);
	PDEBUG( "read %ld bytes from scull device", retval);

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
	PDEBUG( "write %ld bytes to scull device", retval);
	PDEBUG( "%ld bytes are currently in scull device", dev->size);

done:
	up(&dev->sem);

	return retval;
}

/*
 * The ioctl() implementation
 */

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	int retval = 0, tmp;
	switch(cmd) {

	  case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		break;
        
	  case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	  case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		scull_quantum = arg;
		break;

	  case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	  case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	  case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	  case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;
        
	  case SCULL_IOCSQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(scull_qset, (int __user *)arg);
		break;

	  case SCULL_IOCTQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		scull_qset = arg;
		break;

	  case SCULL_IOCGQSET:
		retval = __put_user(scull_qset, (int __user *)arg);
		break;

	  case SCULL_IOCQQSET:
		return scull_qset;

	  case SCULL_IOCXQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_qset;
		retval = __get_user(scull_qset, (int __user *)arg);
		if (retval == 0)
			retval = put_user(tmp, (int __user *)arg);
		break;

	  case SCULL_IOCHQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_qset;
		scull_qset = arg;
		return tmp;

	  default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    	THIS_MODULE,
	.open = 		scull_open,
	.release = 		scull_release,
	.read = 		scull_read,
	.write = 		scull_write,
	.unlocked_ioctl = 		scull_ioctl,
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

#ifdef SCULL_DEBUG
	scull_remove_proc();
#endif

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

#ifdef SCULL_DEBUG
	scull_create_proc();
#endif

	return 0;

fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
