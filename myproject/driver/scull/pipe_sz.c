#include <linux/module.h>

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
//#include <asm/current.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
//#include <linux/semaphore.h>

#include "scull_sz.h"

static const char* name = "scullp_sz";
static dev_t scull_p_devno_sz;
static int scull_p_nr_devs_sz = 4;

int scull_p_buffer_sz = 4000;

typedef struct scull_pipe_sz
{
	wait_queue_head_t inq, outq;
	char* begin, *end;
	int buffer_size;
	char *rp, *wp;
	int nreaders, nwriters;
	struct fasync_struct *async_queue;
	struct semaphore sem;
	struct cdev cdev;
} scull_pipe_sz;

scull_pipe_sz *scull_p_devices_sz;

static ssize_t scull_p_read_sz(struct file *filp, char __user *buf, size_t count,
	loff_t *pos)
{
	scull_pipe_sz *dev = filp->private_data;

	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}

	while(dev->rp == dev->wp)
	{
		up(&dev->sem);
		if(filp->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}

		printk(KERN_ALERT "\"%s\" reading: going to sleep\n",
			current->comm);

		if(wait_event_interruptible(dev->inq, dev->rp != dev->wp))
		{
			return -ERESTARTSYS;
		}
		
		if(down_interruptible(&dev->sem))
		{
			return -ERESTARTSYS;
		}
	}
	

	if(dev->wp > dev->rp)
	{
		count = min(count, (size_t)(dev->wp - dev->rp));
	}
	else
	{
		count = min(count, (size_t)(dev->end - dev->rp));
	}

	if(copy_to_user(buf, dev->rp, count))
	{
		up(&dev->sem);
		return -EFAULT;
	}
	dev->rp += count;
	if(dev->rp == dev->end)
	{
		dev->rp = dev->begin;
	}
	up(&dev->sem);

	wake_up_interruptible(&dev->outq);

	printk(KERN_ALERT "\"%s\" did read %ld bytes\n", 
		current->comm, (long)count);
	return count;
}

static int spacefree_sz(scull_pipe_sz *dev)
{
	if(dev->rp == dev->wp)
	{
		return dev->buffer_size - 1; // if  space free size == buffer_size, then rp == rp, so reduce 1
	}
	return ((dev->rp + dev->buffer_size - dev->wp) % dev->buffer_size) - 1;
}

static int scull_getwritespace_sz(scull_pipe_sz * dev, struct file *filp)
{
	while(0 == spacefree_sz(dev))
	{
		DEFINE_WAIT(wait_sz);

		up(&dev->sem);

		if(filp->f_flags & O_NONBLOCK)
		{
			return -EAGAIN;
		}
		printk(KERN_ALERT "\"%s\" writing: going to sleep\n",
			current->comm);
		prepare_to_wait(&dev->outq, &wait_sz, TASK_INTERRUPTIBLE);
		if(0 == spacefree_sz(dev))
		{
			schedule();
		}
		finish_wait(&dev->outq, &wait_sz);

		if(signal_pending(current))
		{
			return -ERESTARTSYS;
		}
		if(down_interruptible(&dev->sem))
		{
			return -ERESTARTSYS;
		}
	}
	return 0;
}

static int scull_p_open_sz(struct inode *inode, struct file *filp)
{
	scull_pipe_sz *dev;
	dev = container_of(inode->i_cdev, scull_pipe_sz, cdev);
	filp->private_data = dev;

	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
#if 0
	if(!dev->begin)
	{
		dev->begin = kmalloc(scull_p_buffer_sz, GFP_KERNEL);
		if(!dev->begin)
		{
			up(&dev->sem);
			return -ENOMEM;
		}
	}
	dev->buffer_size = scull_p_buffer_sz;
	dev->end = dev->begin + dev->buffer_size;
	dev->rp = dev->wp = dev->begin;
#endif

	if(filp->f_mode & FMODE_READ)
	{
		dev->nreaders++;
	}
	if(filp->f_mode & FMODE_WRITE)
	{
		dev->nwriters++;
	}
	up(&dev->sem);
	return nonseekable_open(inode, filp);
}

static int scull_p_fasync_sz(int fd, struct file *filp, int mode)
{
	printk(KERN_ALERT "%s enter\n", __func__);
	scull_pipe_sz *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int scull_p_release_sz(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "%s enter\n", __func__);
	scull_pipe_sz *dev = filp->private_data;
	scull_p_fasync_sz(-1, filp, 0);
	down(&dev->sem);
	if(filp->f_mode & FMODE_READ)
	{
		dev->nreaders--;
	}
	if(filp->f_mode & FMODE_WRITE)
	{
		dev->nwriters--;
	}
#if 0
	if(0 == (dev->nreaders + dev->nwriters))
	{
		kfree(dev->begin);
		dev->begin = NULL;
	}
#endif
	up(&dev->sem);
	return 0;
}

static ssize_t scull_p_write_sz(struct file *filp, const char __user *buf, size_t count,
	loff_t *pos)
{
	scull_pipe_sz *dev = filp->private_data;
	int result;

	if(down_interruptible(&dev->sem))
	{
		return -ERESTARTSYS;
	}
	result = scull_getwritespace_sz(dev, filp);
	if(result)
	{
		return result;
	}

	count = min(count, (size_t)spacefree_sz(dev));
	if(dev->wp >= dev->rp)
	{
		count = min(count, (size_t)(dev->end - dev->wp));
	}
	else
	{
		count = min(count, (size_t)(dev->rp - dev->wp - 1)); // write to fill up tot rp -1
	}
	printk(KERN_ALERT "Going to accept %ld bytes to %p from %p\n",
		(long)count, dev->wp, buf);
	if(copy_from_user(dev->wp, buf, count))
	{
		up(&dev->sem);
		return -EFAULT;
	}
	dev->wp += count;
	if(dev->wp == dev->end)
	{
		dev->wp = dev->begin;
	}
	up(&dev->sem);

	wake_up_interruptible(&dev->inq);

	if(dev->async_queue)
	{
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	}
	printk(KERN_ALERT "\"%s\" did write %ld bytes\n", 
		current->comm, (long)count);
	return count;
}

static unsigned int scull_p_pull_sz(struct file *filp, poll_table *wait)
{
	printk(KERN_ALERT "%s enter\n", __func__);
	scull_pipe_sz *dev = filp->private_data;
	unsigned int mask = 0;

	down(&dev->sem);
	poll_wait(filp, &dev->inq, wait);
	poll_wait(filp, &dev->outq, wait);
	if(dev->rp != dev->wp)
	{
		mask |= POLLIN | POLLRDNORM; // readable
	}
	else if(spacefree_sz(dev))
	{
		mask |= POLLOUT | POLLWRNORM; // writable
	}
	up(&dev->sem);
	return mask;
}

struct file_operations scull_pipe_fops_sz = {
	.owner = THIS_MODULE,
	.open = scull_p_open_sz,
	.read = scull_p_read_sz,
	.write = scull_p_write_sz,
	.release = scull_p_release_sz,
	.poll = scull_p_pull_sz,
	.fasync = scull_p_fasync_sz,
};

static void scull_p_setup_cdev_sz(scull_pipe_sz *dev, int index)
{
	int err, devno = scull_p_devno_sz + index;

	
	if(!dev->begin)
	{
		dev->begin = kmalloc(scull_p_buffer_sz, GFP_KERNEL);
		if(!dev->begin)
		{
			//return -ENOMEM;
			return;
		}
	}
	dev->buffer_size = scull_p_buffer_sz;
	dev->end = dev->begin + dev->buffer_size;
	dev->rp = dev->wp = dev->begin;

	cdev_init(&dev->cdev, &scull_pipe_fops_sz);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
	{
		printk(KERN_ALERT "Error %d adding scullpipe%d\n", err, index);
	}
}

int scull_p_init_sz(dev_t firstdev)
{
	int i, result;

	result = register_chrdev_region(firstdev, scull_p_nr_devs_sz, name);
	if(result < 0)
	{
		printk(KERN_ALERT "Unable to get scullp region, error = %d\n", result);
		return 0;
	}
	scull_p_devno_sz = firstdev;

	scull_p_devices_sz = kmalloc(scull_p_nr_devs_sz * sizeof(scull_pipe_sz), GFP_KERNEL);
	if(!scull_p_devices_sz)
	{
		unregister_chrdev_region(scull_p_devno_sz, scull_p_nr_devs_sz);
		return 0;
	}

	memset(scull_p_devices_sz, 0, scull_p_nr_devs_sz * sizeof(scull_pipe_sz));

	for(i = 0; i < scull_p_nr_devs_sz; i++)
	{
		init_waitqueue_head(&(scull_p_devices_sz[i].inq));
		init_waitqueue_head(&(scull_p_devices_sz[i].outq));
		init_MUTEX(&scull_p_devices_sz[i].sem);
		scull_p_setup_cdev_sz(&scull_p_devices_sz[i], i);
	}
	
	return scull_p_nr_devs_sz;
}

void scull_p_cleanup_sz(void)
{
	int i;

	if(!scull_p_devices_sz)
	{
		return;
	}
	
	for(i = 0; i < scull_p_nr_devs_sz; i++)
	{
		cdev_del(&scull_p_devices_sz[i].cdev);
		kfree(scull_p_devices_sz[i].begin);
	}
	kfree(scull_p_devices_sz);
	unregister_chrdev_region(scull_p_devno_sz, scull_p_nr_devs_sz);
	scull_p_devices_sz = NULL;
}