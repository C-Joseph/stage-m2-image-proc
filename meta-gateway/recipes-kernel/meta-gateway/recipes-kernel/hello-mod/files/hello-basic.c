#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init hello_start(void)
{
	pr_info("Hello World! I'm here!\n");
	return 0;
}

static void __exit hello_end(void)
{
	pr_info("Goodbye World!\n");
}


module_init(hello_start);
module_exit(hello_end);
MODULE_LICENSE("GPL");
