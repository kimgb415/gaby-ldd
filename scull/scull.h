/*
 * Macros to help debugging
 */
#undef PDEBUG             /* undef it, just in case */
#ifdef SCULL_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "scull: [%s] " fmt, __func__, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, "[%s] " fmt, __func__, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

// it can be used to easily “comment” print statements without removing them entirely.
#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0   /* dynamic major by default */
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4    /* scull0 through scull3 */
#endif

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scull_dev->data" points to an array of pointers, each
 * pointer refers to a memory area of SCULL_QUANTUM bytes.
 *
 * The array (quantum-set) is SCULL_QSET long.
 */
#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET    1000
#endif

#include <linux/cdev.h>
#include <linux/ioctl.h>

/*
 * Representation of scull quantum sets.
 */
struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;              /* the current quantum size */
	int qset;                 /* the current array size */
	unsigned long size;       /* amount of data stored here */
	struct semaphore sem;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};

extern int scull_major;
extern int scull_nr_devs;

/*
 * Prototypes for shared functions
 */
int scull_trim(struct scull_dev *dev);

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define SCULL_IOC_MAGIC  'k'

enum SCULL_IOC_ENUM {
	SCULL_IOCRESET_IDX,
	SCULL_IOCSQUANTUM_IDX,
	SCULL_IOCSQSET_IDX,
	SCULL_IOCTQUANTUM_IDX,
	SCULL_IOCTQSET_IDX,
	SCULL_IOCGQUANTUM_IDX,
	SCULL_IOCGQSET_IDX,
	SCULL_IOCQQUANTUM_IDX,
	SCULL_IOCQQSET_IDX,
	SCULL_IOCXQUANTUM_IDX,
	SCULL_IOCXQSET_IDX,
	SCULL_IOCHQUANTUM_IDX,
	SCULL_IOCHQSET_IDX,

	SCULL_IOC_MAXNR
};

/* Please use a different 8-bit number in your code */

#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC, SCULL_IOCRESET_IDX)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC, SCULL_IOCSQUANTUM_IDX, int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC, SCULL_IOCSQSET_IDX, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,  SCULL_IOCTQUANTUM_IDX)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,  SCULL_IOCTQSET_IDX)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC, SCULL_IOCGQUANTUM_IDX, int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC, SCULL_IOCGQSET_IDX, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,  SCULL_IOCQQUANTUM_IDX)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,  SCULL_IOCQQSET_IDX)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC,SCULL_IOCXQUANTUM_IDX, int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,SCULL_IOCXQSET_IDX, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,  SCULL_IOCHQUANTUM_IDX)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,  SCULL_IOCHQSET_IDX)
