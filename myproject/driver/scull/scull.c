//hello.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
//#include <drm/drm_os_linux.h>

MODULE_LICENSE("Dual BSD/GPL");

static int scull_quantum = 4000;
static int scull_qset = 1000;

int scull_open(struct inode *inode, struct file *filp);

struct file_operations scull_fops = {
	.owner	=	THIS_MODULE,
	.llseek	=	NULL,
	.read	=	NULL,
	.write	= 	NULL,
	//.ioctl	=	NULL,
	.open	=	scull_open,
	.release=	NULL,
};

struct scull_qset
{
	void **data;
	struct scull_qset *next;
};

struct scull_dev
{
	struct scull_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	struct cdev cdev;
};
int scull_trim(struct scull_dev *dev);

int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	int qset = dev->qset;
	int i;
	for(dptr = dev->data; dptr; dptr = next) {
		if(dptr->data) {
			for(i = 0; i < qset; i++) {
				kfree(dptr->data[i]);
			}
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;
	return 0;
}

ssize_t scull_read(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
	struct scull_dev *dev = filep->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	int i;
	ssize_t retval = 0;

	if(*f_pos >= dev->size) {
		goto out;
	}
	
	if(*f_pos + count > dev->size) {
		count = dev->size - *f_pos;
	}
	
	item = (long) *f_pos / itemsize;
	rest = (long) *f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dptr = dev->data;
	for(i = 0; i < item; i++) {
		dptr = dptr->next;
	}
	
	if(NULL == dptr
		|| !dptr->data
		|| !dptr->data[s_pos]) {
		goto out;
	}

	if(count > quantum - q_pos) {
		count = quantum - q_pos;
	}

	if(copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
	out:
		return retval;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev;
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;
	if((filp->f_flags & O_ACCMODE) == O_WRONLY)
	{
		scull_trim(dev);
	}
	return 0;
}

static dev_t dev;
static unsigned int scull_major, scull_minor = 0, scull_count = 1;

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);
	
	cdev_init(&dev->cdev, NULL);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = NULL;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
	{
		printk(KERN_ALERT "Error %d adding scull%d\n", err, index);
	}
	else
	{
		printk(KERN_ALERT "Success devno is:%d adding scull%d\n", devno, index);
	}
}

struct scull_dev scull_dev;

static int scull_init(void)
{
	int result;
	printk(KERN_ALERT "scull_init enter.\n");
	result = alloc_chrdev_region(&dev, scull_minor, scull_count, "scull");  
	scull_major = MAJOR(dev);
	printk(KERN_ALERT "scull_major is:%u\n", scull_major);
	if(result > 0)
	{
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}
	scull_setup_cdev(&scull_dev, 0);
	return 0;
}

static void scull_exit(void)
{
	printk(KERN_ALERT "scull_exit enter.\n");
	cdev_del(&scull_dev.cdev);
	unregister_chrdev_region(dev, scull_count);	
}

module_init(scull_init);
module_exit(scull_exit);
