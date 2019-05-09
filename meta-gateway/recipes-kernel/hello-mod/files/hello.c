/******************************************************************************
 *
 *   Copyright (C) 2011  Intel Corporation. All rights reserved.
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>  /* for put_user */
#include <asm/errno.h>

#define BUF_LEN 80            /* Max length of the message from the device */

static int is_open=0;
int devnum;
static char msg[BUF_LEN];    /* The msg the device will give when asked    */
static char *msg_Ptr;

static ssize_t hello_read (struct file *file_ptr, char *out_buffer, size_t n_bytes, loff_t *offset){
	/* Number of bytes actually written to the buffer */
	int bytes_read = 0;
	if (*msg_Ptr == 0) return 0;

	/* 
	 * Actually put the data into the buffer 
	 */
	while (n_bytes && *msg_Ptr) {

		/* 
		 * The buffer is in the user data segment, not the kernel 
		 * segment so "*" assignment won't work.  We have to use 
		 * put_user which copies data from the kernel data segment to
		 * the user data segment. 
		 */
		put_user(*(msg_Ptr++), out_buffer++);

		n_bytes--;
		bytes_read++;
	}
	/* Most read functions return the number of bytes put into the buffer */
	return bytes_read;
}
static ssize_t hello_write (struct file *file_ptr, const char *in_buffer, size_t n_bytes, loff_t *offset){
	printk (KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}
static int hello_open (struct inode *inode_ptr, struct file *file_ptr){
	static int counter = 0;
	if(is_open == 1){
		printk(KERN_INFO "Error: hello device is already open!\n");
		return -EBUSY;
	}
	is_open = 1;
	//MOD_INC_USE_COUNT;
	sprintf(msg, "I already told you %d times Hello world!\n", counter++);
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);
	return 0;
}
static int hello_release (struct inode *inode_ptr, struct file *file_ptr){
	if(is_open == 0){
		printk(KERN_INFO "Error: hello device is not open!\n");
		return -EBUSY;
	}
	is_open = 0;
	//MOD_DEC_USE_COUNT;
	module_put(THIS_MODULE);
	return 0;
}

struct file_operations fops = {
	read: hello_read,
	write: hello_write,
	open: hello_open,
	release: hello_release
};
		

static int __init hello_start(void)
{
	printk(KERN_INFO "Hello World! I'm here!\n");
	devnum= register_chrdev(0, "hello", &fops); 
	printk(KERN_INFO "Hello device major number : %d\n", devnum);
	return 0;
}

void cleanup_module(void)
{
	printk("Goodbye World!\n");
}

module_init(hello_start);
MODULE_LICENSE("GPL");
