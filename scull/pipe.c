#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h> // kzalloc
#include <linux/wait.h>
#include <linux/semaphore.h>

#include "scull.h"

struct scull_pipe {
    wait_queue_head_t read_queue, write_queue;
    int buffer_size;
    char *buffer_start, *buffer_end;
    int readers_cnt, writers_cnt;
    char *read_pointer, *write_pointer;
    struct semaphore sem;
    struct cdev cdev;
};

static int scull_p_nr_deivces = SCULL_P_NR_DEVS;
static int scull_p_buffer = SCULL_P_BUFFER;
dev_t scull_p_devno;

struct scull_pipe *scull_p_devices = NULL;

static int space_free(struct scull_pipe *dev)
{
    // we leave one free space to indicate the state of full buffer 
    // hence only buffer_size - 1 is available to the device
    if (dev->write_pointer == dev->read_pointer)
        return dev->buffer_size - 1;

    // since the write pointer might behind of read pointer
    return (dev->write_pointer + dev->buffer_size - dev->read_pointer) % dev->buffer_size - 1;
}


static int scull_p_open(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev;
	
	dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
	filp->private_data = dev;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (!dev->buffer_start) {
        dev->buffer_start = kzalloc(dev->buffer_size, GFP_KERNEL);
        if (!dev->buffer_start)
            return -ENOMEM;
        dev->read_pointer = dev->write_pointer = dev->buffer_start;
        dev->buffer_end = dev->buffer_start + dev->buffer_size;
    }

    if (filp->f_flags & FMODE_READ)
        dev->readers_cnt++;
    if (filp->f_flags & FMODE_WRITE)
        dev->writers_cnt++;

    up(&dev->sem);
    return 0;
}


static int scull_p_release(struct inode *inode, struct file *filp)
{
    struct scull_pipe *dev = filp->private_data;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (filp->f_flags & FMODE_READ)
        dev->readers_cnt--;
    if (filp->f_flags & FMODE_WRITE)
        dev->writers_cnt--;

    // discard remaining contents if no more consumers nor producers exist
    if (dev->writers_cnt == 0 && dev->readers_cnt == 0) {
        if (dev->buffer_start)
            kfree(dev->buffer_start);
        dev->buffer_start = NULL;
    }

    up(&dev->sem);
    return 0;
}

ssize_t scull_p_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_pipe *dev = filp->private_data;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    while (dev->write_pointer == dev->read_pointer) {
        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;

        PDEBUG("pipe reader %s is about to sleep\n", current->comm);
        // signal the wait event is interrupted
        if (wait_event_interruptible(dev->read_queue, dev->write_pointer != dev->read_pointer))
            return -ERESTARTSYS; 
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }

    if (dev->write_pointer >= dev->read_pointer) {
        // readers can consume all the content up to where write_pointer is
        count = min(count, (size_t) (dev->write_pointer - dev->read_pointer));
    } else {
        count = min(count,  (size_t) (dev->buffer_end - dev->read_pointer));
    }
    if (copy_to_user(buf, (size_t) dev->read_pointer, count)) {
        // release the semaphore in case of EFAULT
        up(&dev->sem);
        return -EFAULT;
    }
    dev->read_pointer += count;

    // wrap read pointer if reached the end of buffer
    if (dev->read_pointer == dev->buffer_end)
        dev->read_pointer = dev->buffer_start;

    PDEBUG("pipe reader %s read %ld bytes and wakes up writer queue\n", current->comm, count);
    wake_up_interruptible(&dev->write_queue);

    up(&dev->sem);
    return count;
}

ssize_t scull_p_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct scull_pipe *dev = filp->private_data;

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    while (space_free(dev) == 0) {
        up(&dev->sem);
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        PDEBUG("pipe writer %s is about to sleep\n", current->comm);
        if (wait_event_interruptible(dev->write_queue, space_free(dev) > 0))
            return -ERESTARTSYS;
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
    }

    if (dev->write_pointer >= dev->read_pointer) {
        count = min(count, (size_t) (dev->buffer_end - dev->write_pointer));
    } else {
        // writers can only write upto one byte behind the current read_pointer
        count = min(count, (size_t) (dev->read_pointer - dev->write_pointer - 1));
    }
    if (copy_from_user(dev->write_pointer, buf, count)) {
        // release the semaphore in case of EFAULT
        up(&dev->sem);
        return -EFAULT;
    }
    dev->write_pointer += count;

    // wrap write pointer if reached the end of buffer
    if (dev->write_pointer == dev->buffer_end)
        dev->write_pointer = dev->buffer_start;
        
    PDEBUG("pipe writer %s wrote %ld bytes and wakes up reader queue\n", current->comm, count);
    wake_up_interruptible(&dev->read_queue);

    up(&dev->sem);
    return count;
}

static struct file_operations scull_fops = {
    .owner = THIS_MODULE,
    .open = scull_p_open,
    .release = scull_p_release,
    .read = scull_p_read,
    .write = scull_p_write,
};

static void scull_p_setup_cdev(struct scull_pipe *dev, int index)
{
    int devno = scull_p_devno + index;

    cdev_init(&dev->cdev, &scull_fops);
    dev->cdev.owner = THIS_MODULE;
    int err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        pr_warn("Error %d adding scullpipe%d\n", err, index);
}

int scull_p_init(dev_t first_dev)
{
    int result = register_chrdev_region(first_dev, scull_p_nr_deivces, "scullp");
    if (result < 0) {
        pr_warn("Unable to get scullp region, error %d\n", result);
        return 0;
    }
    
    scull_p_devno = first_dev;
    
    // Note that scull_p_devices is array of struct instead of array of struct pointer
    scull_p_devices = kzalloc((sizeof(struct scull_pipe) * scull_p_nr_deivces),GFP_KERNEL);
    if (scull_p_devices == NULL) {
        unregister_chrdev_region(first_dev, scull_p_nr_deivces);
        return 0;    
    }

    for (int i = 0; i < scull_p_nr_deivces; ++i) {
        struct scull_pipe *curr_dev = &scull_p_devices[i];
        sema_init(&curr_dev->sem, 1);
        init_waitqueue_head(&curr_dev->write_queue);
        init_waitqueue_head(&curr_dev->read_queue);
        curr_dev->buffer_size = scull_p_buffer;
        scull_p_setup_cdev(curr_dev, i);
    }

    return scull_p_nr_deivces;
}

void scull_p_cleanup(void)
{
    if (!scull_p_devices)
        return;

    for (int i = 0; i < scull_p_nr_deivces; ++i) {
        struct scull_pipe *curr_dev = &scull_p_devices[i];
        cdev_del(&curr_dev->cdev);
        if (curr_dev->buffer_start) {
            kfree(curr_dev->buffer_start);
            curr_dev->buffer_start = NULL;
        }
    }

    unregister_chrdev_region(scull_p_devno, scull_p_nr_deivces);
    kfree(scull_p_devices);
    scull_p_devices = NULL;

    return;
}