#include <linux/module.h>
//#include <linux/init.h>

#include <linux/sched.h>  /* current and everything */
#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/types.h>  /* size_t */
#include <linux/completion.h>

MODULE_LICENSE("Dual BSD/GPL");
static int complete_major_sz = 0;

DECLARE_COMPLETION(comp_sz);

ssize_t complete_read_sz(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_ALERT "process %d (%s) going to sleep\n",
		current->pid, current->comm);
	wait_for_completion(&comp_sz);
	printk(KERN_ALERT "awoken %d (%s)\n", 
		current->pid, current->comm);
	return 0;
}

ssize_t complete_write_sz(struct file *filp, const char __user *buf, size_t count, loff_t *pos)
{
	printk(KERN_ALERT "process %d (%s) awakening the readers...\n",
		current->pid, current->comm);
	complete(&comp_sz);
	
	return count;
}

struct file_operations complete_oprations_sz = 
{
	.owner = THIS_MODULE,
	.read = complete_read_sz,
	.write = complete_write_sz,
};

static const char *complete_sz = "complete_device_sz";
int complete_init_sz(void)
{
	printk(KERN_ALERT "\n\n\n\n\n%s enter.", __func__);
	int result;
	result = register_chrdev(complete_major_sz, complete_sz, &complete_oprations_sz);
	if(result < 0)
	{
		return result;
	}
	else
	{
		complete_major_sz = result;
	}
	return 0;
}
static void complete_cleanup_sz(void)
{
	printk(KERN_ALERT "%s enter.", __func__);
	unregister_chrdev(complete_major_sz, complete_sz);
}

module_init(complete_init_sz);
module_exit(complete_cleanup_sz);
