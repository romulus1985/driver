#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/wait.h>

MODULE_LICENSE("Dual BSD/GPL");

static int sleepy_major_sz = 0;

static DECLARE_WAIT_QUEUE_HEAD(wq_sz);
static int flag_sz = 0;

ssize_t sleepy_read_sz(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_ALERT "process %d (%s) going to sleep\n",
		current->pid, current->comm);
	wait_event_interruptible(wq_sz, 0 != flag_sz);
	flag_sz = 0;
	printk(KERN_ALERT "awoken %d (%s)\n", 
		current->pid, current->comm);
	return 0;
}

ssize_t sleepy_write_sz(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_ALERT "process %d (%s) awakening the readers...\n",
		current->pid, current->comm);
	flag_sz = 1;
	wake_up_interruptible(&wq_sz);
	return count;
}

struct file_operations sleepy_ops = 
{
	.owner = THIS_MODULE,
	.read = sleepy_read_sz,
	.write = sleepy_write_sz,
};

static const char *chrdev_name = "sleepy_sz";
int sleepy_init_sz(void)
{
	printk(KERN_ALERT "\n\n\n\n %s enter\n", __func__); 
	int result;

	result = register_chrdev(sleepy_major_sz, chrdev_name, &sleepy_ops);
	if(0 > result)
	{
		return result;
	}
	if(0 == sleepy_major_sz)
	{
		sleepy_major_sz = result;
	}
	return 0;
}

void sleepy_cleanup_sz(void)
{
	unregister_chrdev(sleepy_major_sz, chrdev_name);
}

module_init(sleepy_init_sz);
module_exit(sleepy_cleanup_sz);