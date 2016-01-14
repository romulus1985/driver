//hellop.c
#include <linux/init.h>
#include <linux/module.h>

static char *whom = "world";
static int howmany = 1;

module_param(howmany, int, S_IRUGO);
module_param(whom, charp, S_IRUGO);

MODULE_LICENSE("Dual BSD/GPL");

static int hello_init(void)
{
int i;
for(i = 0; i < howmany; i++)
{
	printk(KERN_ALERT "Hello, world by %s at 0910\n", whom);
}
return 0;
}

static void hello_exit(void)
{
printk(KERN_ALERT"Goodbye, cruel world by sunzhao\n");
}

module_init(hello_init);
module_exit(hello_exit);
