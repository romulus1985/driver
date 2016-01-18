//hello.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
//#include <drm/drm_os_linux.h>

MODULE_LICENSE("Dual BSD/GPL");

static int scull_quantum = 4000;
static int scull_qset = 1000;

int scull_open_sz(struct inode *inode, struct file *filp);
ssize_t scull_read_sz(struct file *filep, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write_sz(struct file *filep, const char __user *buf, size_t count, loff_t *f_pos);

struct file_operations scull_fops = {
	.owner	=	THIS_MODULE,
	.llseek	=	NULL,
	.read	=	scull_read_sz,
	.write	= 	scull_write_sz,
	//.ioctl	=	NULL,
	.open	=	scull_open_sz,
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
	struct semaphore sem;
};

int scull_trim(struct scull_dev *dev)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
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

struct scull_qset* scull_follow_sz(struct scull_dev *dev, const int count)
{
	printk(KERN_ALERT "%s enter\n", __func__);
	struct scull_qset* pSet = dev->data;
	struct scull_qset* next;
	int i;
	printk(KERN_ALERT "pSet is:%p\n", pSet);
	if(!pSet)
	{
		pSet = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		// save pointer in data
		dev->data = pSet;
		if(!pSet)
		{
			return NULL;
		}
		memset(pSet, 0, sizeof(struct scull_qset));
	}
	for(i = 0; i < count; i++)
	{
		next = pSet->next;
		if(!next)
		{
			next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if(!next)
			{
				return NULL;
			}
			memset(next, 0, sizeof(struct scull_qset));
		}
		pSet = next;
	}
	return pSet;
}

ssize_t scull_write_sz(struct file * filep, const char __user *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_ALERT "%s enter. f_pos is:%ld\n", __func__, (long)*f_pos);
	struct scull_dev *dev = filep->private_data;
	printk(KERN_ALERT "dev is:%p.\n", dev);
	struct scull_qset *dptr = NULL;
	int quantum = dev->quantum;
	int qset = dev->qset;
	const int item_size = qset * quantum;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM;
	//printk(KERN_ALERT "before down_interruptible at %d.\n", __LINE__);
	if(down_interruptible(&dev->sem)) 
	{
		return -ERESTARTSYS;
	}
	//printk(KERN_ALERT "after down_interruptible at %d.\n", __LINE__);
	/* if has no long, comile warning:
	* WARNING: "__divdi3" [/home/romulus/driver/myproject/driver/scull/scull.ko] undefined!
	*/
	item = (long)*f_pos / item_size;
	rest = (long)*f_pos % item_size;
	s_pos = rest / qset;
	q_pos = rest % qset;
	printk(KERN_ALERT "item is:%d qset is:%d s_pos is:%d at %d.\n",
		item, qset, s_pos, __LINE__);

	dptr = scull_follow_sz(dev, item);
	//printk(KERN_ALERT "after scull_follow_sz at %d.\n", __LINE__);
	if(!dptr)
	{
		goto out;
	}
	if(!dptr->data) 
	{
		printk(KERN_ALERT "!dptr->data case, dptr is:%p at %d.\n", dptr, __LINE__);
		dptr->data = kmalloc(qset * sizeof(char*), GFP_KERNEL);
		if(!dptr->data)
		{
			goto out;
		}
		memset(dptr->data, 0, qset * sizeof(char*));
	}
	printk(KERN_ALERT "after dptr->data kmalloc at %d.\n", __LINE__);
	printk(KERN_ALERT "dptr->data is:%p\n", dptr->data);
	printk(KERN_ALERT "dptr->data[%d] is:%p\n", s_pos, dptr->data[s_pos]);
	if(!dptr->data[s_pos])
	{
		printk(KERN_ALERT "!dptr->data[s_pos] at %d.\n", __LINE__);
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if(!dptr->data[s_pos])
		{
			goto out;
		}
		memset(dptr->data[s_pos], 0, quantum);
	}
	printk(KERN_ALERT "after dptr->data[s_pos] kmalloc at %d.\n", __LINE__);
	if(q_pos + count > quantum)
	{
		count = quantum - q_pos;
	}
	if(copy_from_user(dptr->data[s_pos] + q_pos, buf, count))
	{
		retval = -EFAULT;
		goto out;
	}
	*f_pos = *f_pos + count;
	retval = count;
	if(*f_pos > dev->size)
	{
		dev->size = *f_pos;
	}
	out:
		up(&dev->sem);
		printk(KERN_ALERT "write count is:%d\n", retval);
		return retval;
}

ssize_t scull_read_sz(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
	ssize_t retval = 0;
	struct scull_dev *dev = filep->private_data;
	printk(KERN_ALERT "dev is:%p.\n", dev);
	struct scull_qset *dptr;
	int quantum = dev->quantum;
	int qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	//int i;

	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}

	printk(KERN_ALERT "f_pos is:%ld dev->size is:%lu at %d.\n", 
		(long)*f_pos, dev->size, __LINE__);
	if(*f_pos >= dev->size) {
		goto out;
	}
	
	if(*f_pos + count > dev->size) {
		count = dev->size - *f_pos;
	}
	printk(KERN_ALERT "count is:%d at %d.\n", count, __LINE__);
	
	item = (long) *f_pos / itemsize;
	rest = (long) *f_pos % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	#if 0
	dptr = dev->data;
	for(i = 0; i < item; i++) {
		dptr = dptr->next;
	}
	#endif
	printk(KERN_ALERT "before scull_follow_sz, item is:%d at %d.\n", item, __LINE__);
	dptr = scull_follow_sz(dev, item);
	printk(KERN_ALERT "after scull_follow_sz dptr is:%p at %d .\n", dptr, __LINE__);
	if(dptr)
	{
		printk(KERN_ALERT "dptr->data is:%p at %d.\n", dptr->data, __LINE__);
		if(dptr->data)
		{
			printk(KERN_ALERT "dptr->data[%d] is:%p at %d.\n", 
				s_pos, dptr->data[s_pos], __LINE__);
		}
	}
	if(NULL == dptr
		|| !dptr->data
		|| !dptr->data[s_pos]) {
		printk(KERN_ALERT "goto out at %d.\n", __LINE__);
		goto out;
	}
	printk(KERN_ALERT "dptr data data[s_pos] is not null.\n");

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
		up(&dev->sem);
		printk(KERN_ALERT "read count is:%d\n", retval);
		return retval;
}

int scull_open_sz(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
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

static void scull_setup_cdev_sz(struct scull_dev *dev, int index)
{
	printk(KERN_ALERT "%s enter. dev is:%p\n", __func__, dev);
	int err, devno = MKDEV(scull_major, scull_minor + index);
	
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
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

struct scull_dev my_dev;

int scull_read_procmem_sz(char * buf, char * * start, off_t offset, int count, int * eof, void * data)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
	int i, j, len = 0;
	int limit = count - 80;
	printk(KERN_ALERT "run at line %d\n", __LINE__);
	struct scull_dev *dev = &my_dev;
	printk(KERN_ALERT "dev is:%p\n", dev);
	struct scull_qset *qset = dev->data;
	printk(KERN_ALERT, "dev is:%p, qset is:%p\n", dev, qset);
	printk(KERN_ALERT "dev2 is:%p\n", dev);
	
	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	len += sprintf(buf+len, "\nDevice 0: qset %i, q %i, size %li\n",
		dev->qset, dev->quantum, dev->size);
	for(; len < limit; qset = qset->next)
	{
		len += sprintf(buf + len, "\n item at %p, qset at %p\n",
			qset, qset->data);
		if(qset->data 
			&& !qset->next) // last qset
		{
			for(j = 0; j < dev->qset&& len < limit; j++)
			{
				if(qset->data[j])
				{
					len += sprintf(buf+len, "    %4i: %8p",
						j, qset->data[j]);
				}
			}
		}
	}
	up(&dev->sem);
	*eof = 1;
	return len;
}

static const char* proc_mem_name = "scull_mem_sz";
static void scull_create_proc_sz(void)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
	create_proc_read_entry(proc_mem_name,
		0,
		NULL,
		scull_read_procmem_sz,
		NULL);
}

//#define SCULL_DEBUG
static int scull_init_sz(void)
{
	int result;
	printk(KERN_ALERT "%s enter.\n", __func__);
	result = alloc_chrdev_region(&dev, scull_minor, scull_count, "scull_devices");  
	scull_major = MAJOR(dev);
	printk(KERN_ALERT "scull_major is:%u\n", scull_major);
	if(result > 0)
	{
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}
	init_MUTEX(&my_dev.sem);
	scull_setup_cdev_sz(&my_dev, 0);
#ifdef SCULL_DEBUG
	scull_create_proc_sz();
#endif
	return 0;
}

static void scull_exit_sz(void)
{
	printk(KERN_ALERT "%s exit.\n", __func__);
#ifdef SCULL_DEBUG
	remove_proc_entry(proc_mem_name, NULL);
#endif
	cdev_del(&my_dev.cdev);
	unregister_chrdev_region(dev, scull_count);	
}

module_init(scull_init_sz);
module_exit(scull_exit_sz);
