//hello.c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include "scull_ioctl.h"

//#include <drm/drm_os_linux.h>

#include "scull_sz.h"

MODULE_LICENSE("Dual BSD/GPL");

#define SCULL_QUANTUM 4000
#define SCULL_QSET 1000

static int scull_quantum = 4000;
static int scull_qset = 1000;
static int scull_nr_devs_sz = 4;
//int scull_p_buffer_sz = 4000;

int scull_open_sz(struct inode *inode, struct file *filp);
ssize_t scull_read_sz(struct file *filep, char __user *buf, size_t count, loff_t *f_pos);
ssize_t scull_write_sz(struct file *filep, const char __user *buf, size_t count, loff_t *f_pos);
static void scull_exit_sz(void);

static int scull_ioctl_sz(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg);

struct file_operations scull_fops = {
	.owner	=	THIS_MODULE,
	.llseek	=	NULL,
	.read	=	scull_read_sz,
	.write	= 	scull_write_sz,
	.ioctl	=	scull_ioctl_sz,
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
struct scull_dev* scull_devices;

static void *scull_seq_start(struct seq_file *s, loff_t *pos)
{
	if(*pos >= scull_nr_devs_sz)
	{
		return NULL;
	}
	return scull_devices + *pos;
}

static void *scull_seq_next(struct seq_file *s, void* v, loff_t *pos)
{
	(*pos)++;
	if(*pos >= scull_nr_devs_sz)
	{
		return NULL;
	}
	return scull_devices + *pos;
}

static void scull_seq_stop(struct seq_file *s, void* v)
{

}

static int scull_seq_show(struct seq_file *s, void* v)
{
	struct scull_dev *dev = (struct scull_dev *)v;
	struct scull_qset *qset = NULL;
	int i;
	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	seq_printf(s, "Device %d: qset %d, quantum %d, size %ld for proc seq\n",
		(int)(dev - scull_devices), dev->qset, dev->quantum, dev->size);
	for(qset = dev->data; qset; qset = qset->next)
	{
		seq_printf(s, "item at %p, qset at %p\n", qset, qset->data);
		if(qset->data 
			&& !qset->next)
		{
			for(i = 0; i < dev->qset; i++)
			{
				if(qset->data[i])
				{
					seq_printf(s, "    %4i:  %8p\n",
						i, qset->data[i]);
				}
			}
		}
	}
	up(&dev->sem);
	return 0;
}

static struct seq_operations scull_seq_ops = {
	.start = scull_seq_start,
	.next = scull_seq_next,
	.stop = scull_seq_stop,
	.show = scull_seq_show
};

static int scull_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scull_seq_ops);
}

static struct file_operations scull_proc_ops = {
	.owner = THIS_MODULE,
	.open = scull_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
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
	printk(KERN_ALERT "after dptr->data[%d] kmalloc at %d.\n", s_pos, __LINE__);
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

static int scull_ioctl_sz(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int tmp;
	int retval = 0;

	if(_IOC_TYPE(cmd) != SCULL_IOC_MAGIC_SZ)
	{
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > SCULL_IOC_MAXNR_SZ)
	{
		return -ENOTTY;
	}
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if(err) 
	{
		return -EFAULT;
	}
	switch(cmd)
	{
	case SCULL_IOC_RESET_SZ:
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		printk(KERN_ALERT "SCULL_IOC_RESET_SZ: scull_quantum = %d, scull_qset = %d\n",
			scull_quantum, scull_qset);
		break;
	case SCULL_IOC_SQUANTUM_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		retval = __get_user(scull_quantum, (int __user*)arg);
		printk(KERN_ALERT "SCULL_IOC_SQUANTUM_SZ: scull_quantum = %d\n", scull_quantum);
		break;
	case SCULL_IOC_TQUANTUM_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		scull_quantum = arg;
		printk(KERN_ALERT "SCULL_IOC_TQUANTUM_SZ: scull_quantum= %d\n", scull_quantum);
		break;
	case SCULL_IOC_GQUANTUM_SZ:
		retval = __put_user(scull_quantum, (int __user*)arg);
		printk(KERN_ALERT "SCULL_IOC_GQUANTUM_SZ: use arg return scull_quantum = %d\n",
			scull_quantum);
		break;
	case SCULL_IOC_QQUANTUM_SZ:
		retval = scull_quantum;
		printk(KERN_ALERT "SCULL_IOC_QQUANTUM_SZ: return scull_quantum = %d\n",
			scull_quantum);
		break;
	case SCULL_IOC_XQUANTUM_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user*)arg);
		if(0 == retval)
		{
			retval = __put_user(tmp, (int __user*)arg);
		}
		printk(KERN_ALERT "SCULL_IOC_XQUANTUM_SZ: scull_quantum = %d, and use arg return old scull_quantum = %d\n",
			scull_quantum, tmp);
		break;
	case SCULL_IOC_HQUANTUM_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		tmp = scull_quantum;
		scull_quantum = arg;
		retval = tmp;
		printk(KERN_ALERT "SCULL_IOC_HQUANTUM_SZ: scull_quantum = %d, and return old scull_quantum = %d\n",
			scull_quantum, tmp);
		break;
	case SCULL_IOC_SQSET_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		retval = __get_user(scull_qset, (int __user*)arg);
		printk(KERN_ALERT "SCULL_IOC_SQSET_SZ: scull_qset = %d\n", scull_qset);
		break;
	case SCULL_IOC_TQSET_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		scull_qset = arg;
		printk(KERN_ALERT "SCULL_IOC_TQSET_SZ: scull_qset = %d\n", scull_qset);
		break;
	case SCULL_IOC_GQSET_SZ:
		retval = __put_user(scull_qset, (int __user*)arg);
		printk(KERN_ALERT "SCULL_IOC_GQSET_SZ: scull_qset = %d\n", scull_qset);
		break;
	case SCULL_IOC_QQSET_SZ:
		retval = scull_qset;
		printk(KERN_ALERT "SCULL_IOC_QQSET_SZ: scull_qset = %d\n", scull_qset);
		break;
	case SCULL_IOC_XQSET_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		tmp = scull_qset;
		retval = __get_user(scull_qset, (int __user*)arg);
		if(!retval)
		{
			retval = __put_user(tmp, (int __user*)arg);
		}
		printk(KERN_ALERT "SCULL_IOC_XQSET_SZ: scull_qset = %d, and use arg return old scull_qset = %d\n",
			scull_qset, tmp);
		break;
	case SCULL_IOC_HQSET_SZ:
		if(!capable(CAP_SYS_ADMIN))
		{
			return -EPERM;
		}
		tmp = scull_qset;
		scull_qset = arg;
		retval = tmp;
		printk(KERN_ALERT "SCULL_IOC_HQSET_SZ: scull_qset = %d, and return old scull_qset = %d\n",
			scull_qset, tmp);
		break;
	case SCULL_P_IOC_TSIZE_SZ:
		scull_p_buffer_sz = arg;
		break;
	case SCULL_P_IOC_QSIZE_SZ:
		retval = scull_p_buffer_sz;
		break;
	default:
		retval = -EFAULT;
		break;
	}
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

static dev_t devno;
//static unsigned int scull_major, scull_minor = 0, scull_count = 1;
static unsigned int scull_major, scull_minor = 0;

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

int scull_read_procmem_sz(char * buf, char * * start, off_t offset, int count, int * eof, void * data)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
	int i, j, len = 0;
	int limit = count - 80;
	//struct scull_dev *dev = &my_dev;
	for(i = 0; i < scull_nr_devs_sz && len < limit; i++) {
		struct scull_dev *dev = &scull_devices[i];
	printk(KERN_ALERT "dev is:%p\n", dev);
	struct scull_qset *qset = dev->data;
	printk(KERN_ALERT "qset is:%p\n", qset); // why not print???, because KERN_ALERT suffix with ,
	printk(KERN_ALERT "dev2 is:%p\n", dev);
	
	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	len += sprintf(buf+len, "\nDevice %i: qset %i, q %i, size %li for proc mem\n",
		i, dev->qset, dev->quantum, dev->size);
	for(; qset && len < limit; qset = qset->next)
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
					len += sprintf(buf+len, "    %4i: %8p\n",
						j, qset->data[j]);
				}
			}
		}
	}
	up(&dev->sem);
	}
	*eof = 1;
	return len;
}

static const char* proc_mem_name = "scull_mem_sz";
static const char* proc_seq_name = "scull_seq_sz";
static void scull_create_proc_sz(void)
{
	printk(KERN_ALERT "%s enter.\n", __func__);
	struct proc_dir_entry *entry;
	
	create_proc_read_entry(proc_mem_name,
		0,
		NULL,
		scull_read_procmem_sz,
		NULL);

	entry = create_proc_entry(proc_seq_name, 0, NULL);
	if(entry)
	{
		entry->proc_fops = &scull_proc_ops;
	}
}

//#define SCULL_DEBUG
static int scull_init_sz(void)
{
	int result, i;
	printk(KERN_ALERT "\n\n\n%s enter.\n", __func__);
	//result = alloc_chrdev_region(&devno, scull_minor, scull_count, "scull_devices");  
	result = alloc_chrdev_region(&devno, scull_minor, scull_nr_devs_sz, "scull_devices");  
	scull_major = MAJOR(devno);
	printk(KERN_ALERT "scull_major is:%u\n", scull_major);
	if(result > 0)
	{
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	scull_devices = kmalloc(scull_nr_devs_sz * sizeof(struct scull_dev), GFP_KERNEL);
	if(!scull_devices)
	{
		result = -ENOMEM;
		goto fail;
	}
	memset(scull_devices, 0, scull_nr_devs_sz * sizeof(struct scull_dev));

	for(i = 0; i < scull_nr_devs_sz; i++) {
		init_MUTEX(&scull_devices[i].sem);
		scull_devices[i].qset = scull_qset;
		scull_devices[i].quantum = scull_quantum;
		scull_setup_cdev_sz(&scull_devices[i], i);
	}

	devno = MKDEV(scull_major, scull_minor + scull_nr_devs_sz);
	devno += scull_p_init_sz(devno);
	
#ifdef SCULL_DEBUG
	scull_create_proc_sz();
#endif
	return 0;
fail:
	scull_exit_sz();
	return result;
}

static void scull_exit_sz(void)
{
	printk(KERN_ALERT "%s exit.\n", __func__);
	int i;
	dev_t devno = MKDEV(scull_major, scull_minor);
#ifdef SCULL_DEBUG
	remove_proc_entry(proc_mem_name, NULL);
	remove_proc_entry(proc_seq_name, NULL);
#endif
	if(scull_devices) {
		for(i = 0; i < scull_nr_devs_sz; i++)
		{
			scull_trim(&scull_devices[i]);
			//cdev_del(&my_dev.cdev);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}
	//unregister_chrdev_region(devno, scull_count);	
	unregister_chrdev_region(devno, scull_nr_devs_sz);	
	
	scull_p_cleanup_sz();
}

module_init(scull_init_sz);
module_exit(scull_exit_sz);
