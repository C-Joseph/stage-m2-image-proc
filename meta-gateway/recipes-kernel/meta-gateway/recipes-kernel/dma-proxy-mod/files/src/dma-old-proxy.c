/* DMA Proxy
 *
 * This module is designed to be a small example of a DMA device driver that is
 * a client to the DMA Engine using the AXI DMA driver. It serves as a proxy for
 * kernel space DMA control to a user space application.
 *
 * A zero copy scheme is provided by allowing user space to mmap a kernel allocated
 * memory region into user space, referred to as a proxy channel interface. The
 * ioctl function is provided to start a DMA transfer which then blocks until the
 * transfer is complete. No input arguments are being used in the ioctl function.
 *
 * There is an associated user space application, dma_proxy_test.c, and dma_proxy.h
 * that works with this device driver.
 *
 * The hardware design was tested with an AXI DMA without scatter gather and
 * with the transmit channel looped back to the receive channel.
 *
 * This driver is character driver which creates 2 devices that user space can
 * access for each DMA channel, /dev/dma_proxy_rx and /dev/dma_proxy_tx.

 * An internal test mode is provided to allow it to be self testing without the 
 * need for a user space application and this mode is good for making bigger
 * changes to this driver.
 *
 * This driver is designed to be simple to help users get familiar with how to 
 * use the DMA driver provided by Xilinx which uses the Linux DMA Engine. The 
 * performance of this example is not expected to be high without more work.
 *
 * To use this driver a node must be added into the device tree.  Add the 
 * following node while adjusting the dmas property to match the name of 
 * the AXI DMA node.
 * 
 * dma_proxy {
 *    compatible ="xlnx,dma_proxy";
 *    dmas = <&axi_dma_0 0
 *            &axi_dma_0 1>;
 *    dma-names = "dma_proxy_tx", "dma_proxy_rx";
 * }
 *
 */

#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>//kernel memory allocation functions
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_dma.h>

#include "dma-proxy.h"

#undef INTERNAL_TEST

MODULE_LICENSE("GPL");

#define DRIVER_NAME 		"dma_proxy"
#define CHANNEL_COUNT 		2
#define TX_CHANNEL		0
#define RX_CHANNEL		1
#define ERROR 			-1
#define NOT_LAST_CHANNEL 	0
#define LAST_CHANNEL 		1

static unsigned int frm_cnt;

static struct xilinx_vdma_config *vdma_cfg;//Configuration structure for xilinx VDMA driver

static struct dma_chan *dchan;//Used in create_channel function for channel configuration

/* The following module parameter controls if the internal test runs when the module is inserted.
 */
static unsigned internal_test = 0;
module_param(internal_test, int, S_IRUGO);

/* The following data structure represents a single channel of DMA, transmit or receive in the case
 * when using AXI DMA.  It contains all the data to be maintained for the channel.
 */
struct dma_proxy_channel {
	struct dma_proxy_channel_interface *interface_p;	/* user to kernel space interface */
	dma_addr_t interface_phys_addr;

	struct device *proxy_device_p;				/* character device support */
	struct device *dma_device_p;
	dev_t dev_node;
	struct cdev cdev;
	struct class *class_p;

	struct dma_chan *channel_p;				/* dma support */
	struct completion cmp;
	dma_cookie_t cookie;
	dma_addr_t dma_handle;
	u32 direction;						/* DMA_MEM_TO_DEV or DMA_DEV_TO_MEM */
	struct scatterlist sglist;
};

/* Allocate the channels for this example statically rather than dynamically for simplicity.
 */
static struct dma_proxy_channel channels[CHANNEL_COUNT];

/* Handle a callback and indicate the DMA transfer is complete to another
 * thread of control
 */
static void sync_callback(void *completion)
{
	/* Indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */
	complete(completion);
}

/* Prepare a DMA buffer to be used in a DMA transaction, submit it to the DMA engine
 * to ibe queued and return a cookie that can be used to track that status of the
 * transaction
 */
static void start_transfer(struct dma_proxy_channel *pchannel_p/*_rx, struct dma_proxy_channel *pchannel_p_tx*/)
{
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	struct dma_proxy_channel_interface *interface_p = pchannel_p->interface_p;
	struct dma_device *dma_device = pchannel_p->channel_p->device;

	// For now use a single entry in a scatter gather list just for future
	 // flexibility for scatter gather.
	 
	printk("\n**1\n");
	sg_init_table(&pchannel_p->sglist, 1);
	printk("\n**2\n");
	sg_dma_address(&pchannel_p->sglist) = pchannel_p->dma_handle;
	printk("\n**3\n");
	sg_dma_len(&pchannel_p->sglist) = interface_p->length;
	printk("\n**4\n");
	//Error is from this function
	chan_desc = dma_device->device_prep_slave_sg(pchannel_p->channel_p, &pchannel_p->sglist, 1, pchannel_p->direction, flags, NULL);
	//chan_desc = dma_device->device_prep_dma_memcpy(dma_chan *channel, dma_addr_t *dest, dma_addr_t *src, size_t len, unsigned long flags);
	//printk("\n**5\n");
	//chan_desc = dma_device->device_prep_dma_memcpy(pchannel_p->channel_p, (pchannel_p->dma_handle)+(dma_addr_t) (0x00000030), pchannel_p->dma_handle, TEST_SIZE, flags);
	//chan_desc = dma_device->device_prep_dma_memcpy(pchannel_p_rx->channel_p, pchannel_p_tx->dma_handle, pchannel_p_rx->dma_handle, TEST_SIZE, flags);
	//chan_desc = dma_device->dmaengine_prep_slave_single(pchannel_p->channel_p, pchannel_p->dma_handle, TEST_SIZE, pchannel_p->direction, flags);
	//printk("\n**2\n");
	//chan_desc=dmaengine_prep_dma_memcpy();
	// Make sure the operation was completed successfully 
	if (!chan_desc) {
		printk(KERN_ERR "dmaengine_prep*() error\n");
	} else {
		chan_desc->callback = sync_callback;
		printk("\n**3\n");
		chan_desc->callback_param = &pchannel_p->cmp;//&pchannel_p_rx->cmp;
		printk("\n**4\n");

		// Initialize the completion for the transfer and before using it
		// then submit the transaction to the DMA engine so that it's queued
		// up to be processed later and get a cookie to track it's status
		 
		init_completion(&pchannel_p->cmp);
		printk("\n**5\n");

		pchannel_p->cookie = dmaengine_submit(chan_desc);
		printk("\n**6\n");
		if (dma_submit_error(pchannel_p->cookie/*pchannel_p_rx->cookie*/)) {
			printk("Submit error\n");
	 		return;
		}

		// Start the DMA transaction which was previously queued up in the DMA engine
		 
		dma_async_issue_pending(pchannel_p->channel_p/*pchannel_p_rx->channel_p*/);
	}
}

/* Wait for a DMA transfer that was previously submitted to the DMA engine
 */
static void wait_for_transfer(struct dma_proxy_channel *pchannel_p)
{
	unsigned long timeout = msecs_to_jiffies(3000);
	enum dma_status status;

	printk("Waiting for transfer\n");//
	pchannel_p->interface_p->status = PROXY_BUSY;
	printk("Busy status set\n");//

	/* Wait for the transaction to complete, or timeout, or get an error
	 */
	timeout = wait_for_completion_timeout(&pchannel_p->cmp, timeout);
	printk("**Got wait for completion timeout\n");
	status = dma_async_is_tx_complete(pchannel_p->channel_p, pchannel_p->cookie, NULL, NULL);

	printk("Checking timeout and status\n");//

	if (timeout == 0)  {
		pchannel_p->interface_p->status  = PROXY_TIMEOUT;
		printk(KERN_ERR "DMA timed out\n");
	} else if (status != DMA_COMPLETE) {
		pchannel_p->interface_p->status = PROXY_ERROR;
		printk(KERN_ERR "DMA returned completion callback status of: %s\n",
			   status == DMA_ERROR ? "error" : "in progress");
	} else
		pchannel_p->interface_p->status = PROXY_NO_ERROR;
}

/* Perform the DMA transfer for the channel, starting it then blocking to wait for completion.
 */
static void transfer(struct dma_proxy_channel *pchannel_p)
{
	/* The physical address of the buffer in the interface is needed for the dma transfer
	 * as the buffer may not be the first data in the interface
	 */
	//struct dma_proxy_channel *pchannel_p_tx;
	//*pchannel_p_tx = channels[TX_CHANNEL];
	pchannel_p->dma_handle = (dma_addr_t)(pchannel_p->interface_phys_addr + 
					offsetof(struct dma_proxy_channel_interface, buffer));
	//pchannel_p_tx->dma_handle = (dma_addr_t)(pchannel_p_tx->interface_phys_addr + 
	//				offsetof(struct dma_proxy_channel_interface, buffer));
	printk("Transfer func. Dma_handle set done.\n");
	start_transfer(pchannel_p/*, pchannel_p_tx*/);
	printk("Transfer started\n");
	printk("Waiting for transfer\n");
	wait_for_transfer(pchannel_p);
}

/* The following functions are designed to test the driver from within the device
 * driver without any user space.
 */
//------Modif Cleo----//
//static void tx_test(struct work_struct *unused)
//{
//	channels[TX_CHANNEL].interface_p->length = TEST_SIZE;
//	transfer(&channels[TX_CHANNEL]);
//}
static void rx_test(struct work_struct *unused)
{
	channels[RX_CHANNEL].interface_p->length = TEST_SIZE;
	transfer(&channels[RX_CHANNEL]);
	printk("Transfer RX\n");//
}
static void test(void)
{
	int i; unsigned char compare_buf[TEST_SIZE];
	struct work_struct work;
	
	printk("Starting internal test\n");

	/* Initialize the buffers for the test
	 */
	//------Modif Cleo----//
	//Set tx channel to zero and fill rx channel with numbers
	//for (i = 0; i < TEST_SIZE; i++) {
	//	channels[TX_CHANNEL].interface_p->buffer[i] = i;
	//	channels[RX_CHANNEL].interface_p->buffer[i] = 0;
	//}
	for (i = 0; i < TEST_SIZE; i++) {
		channels[TX_CHANNEL].interface_p->buffer[i] = 0;
		channels[RX_CHANNEL].interface_p->buffer[i] = i;
		compare_buf[i] = i;
	}
	

	/* Since the transfer function is blocking the transmit channel is started from a worker
	 * thread
	 */
	//------Modif Cleo----//
	//INIT_WORK(&work, tx_test);
	printk("Starting transfer...\n");//
	INIT_WORK(&work, rx_test);
	printk("Scheduling...\n");//
	//schedule_work(&work);
	if(schedule_work(&work)) printk("Scheduled!\n");
	else if(!schedule_work(&work))printk("Already scheduled!\n");//
	//printk("Scheduled!\n");//
	/*for (i = 0; i < TEST_SIZE; i++) {
		//channels[TX_CHANNEL].interface_p->buffer[i] = 0;
		channels[RX_CHANNEL].interface_p->buffer[i] = i;
	}*/
	//int compare_buf[TEST_SIZE];
	//compare_buf=channels[RX_CHANNEL].interface_p->buffer;//

	/* Receive the data that was just sent and looped back
	 */
	//------Modif Cleo----//
	//No loopback
	//channels[1].interface_p->length = TEST_SIZE;
	//transfer(&channels[1]);

	/* Verify the receiver buffer matches the transmit buffer to
	 * verify the transfer was good
	 */
	//------Modif Cleo----//
	printk("Comparing...\n");//
	printk("Test : %d : %d, %d : %d\n", channels[RX_CHANNEL].interface_p->buffer[0],
	channels[TX_CHANNEL].interface_p->buffer[0], 
	channels[RX_CHANNEL].interface_p->buffer[TEST_SIZE-1], 
	channels[TX_CHANNEL].interface_p->buffer[TEST_SIZE-1]);//
	for (i = 0; i < TEST_SIZE; i++)
		if (channels[TX_CHANNEL].interface_p->buffer[i] !=
			//channels[RX_CHANNEL].interface_p->buffer[i]) {
			compare_buf[i]) {
			printk("buffers not equal, first index = %d\n", i);
			break;
		}
	/*for (i = 0; i < TEST_SIZE; i++)
		if (channels[TX_CHANNEL].interface_p->buffer[i] !=i) {
			printk("buffers not equal, first index = %d\n", i);
			break;
		}*/


	printk("Internal test complete\n");
}

/* Map the memory for the channel interface into user space such that user space can
 * access it using coherent memory which will be non-cached for s/w coherent systems
 * such as Zynq 7K or the current default for Zynq MPSOC. MPSOC can be h/w coherent
 * when set up and then the memory will be cached.
 */
static int mmap(struct file *file_p, struct vm_area_struct *vma)
{
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file_p->private_data;

	return dma_mmap_coherent(pchannel_p->dma_device_p, vma,
					   pchannel_p->interface_p, pchannel_p->interface_phys_addr,
					   vma->vm_end - vma->vm_start);
}

/* Open the device file and set up the data pointer to the proxy channel data for the
 * proxy channel such that the ioctl function can access the data structure later.
 */
static int local_open(struct inode *ino, struct file *file)
{
	file->private_data = container_of(ino->i_cdev, struct dma_proxy_channel, cdev);

	return 0;
}

/* Close the file and there's nothing to do for it
 */
static int release(struct inode *ino, struct file *file)
{
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	struct dma_device *dma_device = pchannel_p->channel_p->device;

	/* Stop all the activity when the channel is closed assuming this
	 * may help if the application is aborted without normal closure
	 */

	dma_device->device_terminate_all(pchannel_p->channel_p);
	return 0;
}

/* Perform I/O control to start a DMA transfer.
 */
static long ioctl(struct file *file, unsigned int unused , unsigned long arg)
{
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	if (pchannel_p == (struct dma_proxy_channel *) NULL) {
		printk("**Null value for dma proxy channel at IOCTL call\n");
		return -1;
	}else printk("**Non-null value for dma proxy channel at IOCTL call\n");

	/* Perform the DMA transfer on the specified channel blocking til it completes
	 */
	transfer(pchannel_p);

	return 0;
}
static struct file_operations dm_fops = {
	.owner    = THIS_MODULE,
	.open     = local_open,
	.release  = release,
	.unlocked_ioctl = ioctl,
	.mmap	= mmap
};


/* Initialize the driver to be a character device such that is responds to
 * file operations.
 */
static int cdevice_init(struct dma_proxy_channel *pchannel_p, char *name)
{
	int rc;
	char device_name[32] = "dma_proxy";
	static struct class *local_class_p = NULL;

	/* Allocate a character device from the kernel for this driver.
	 */
	rc = alloc_chrdev_region(&pchannel_p->dev_node, 0, 1, "dma_proxy");

	if (rc) {
		dev_err(pchannel_p->dma_device_p, "unable to get a char device number\n");
		return rc;
	}

	/* Initialize the device data structure before registering the character 
	 * device with the kernel.
	 */
	cdev_init(&pchannel_p->cdev, &dm_fops);
	pchannel_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&pchannel_p->cdev, pchannel_p->dev_node, 1);

	if (rc) {
		dev_err(pchannel_p->dma_device_p, "unable to add char device\n");
		goto init_error1;
	}

	/* Only one class in sysfs is to be created for multiple channels,
	 * create the device in sysfs which will allow the device node
	 * in /dev to be created
	 */
	if (!local_class_p) {
		local_class_p = class_create(THIS_MODULE, DRIVER_NAME);

		if (IS_ERR(pchannel_p->dma_device_p->class)) {
			dev_err(pchannel_p->dma_device_p, "unable to create class\n");
			rc = ERROR;
			goto init_error2;
		}
	}
	pchannel_p->class_p = local_class_p;

	/* Create the device node in /dev so the device is accessible
	 * as a character device
	 */
	strcat(device_name, name);
	pchannel_p->proxy_device_p = device_create(pchannel_p->class_p, NULL,
					  	 pchannel_p->dev_node, NULL, name);

	if (IS_ERR(pchannel_p->proxy_device_p)) {
		dev_err(pchannel_p->dma_device_p, "unable to create the device\n");
		goto init_error3;
	}

	return 0;

init_error3:
	class_destroy(pchannel_p->class_p);

init_error2:
	cdev_del(&pchannel_p->cdev);

init_error1:
	unregister_chrdev_region(pchannel_p->dev_node, 1);
	return rc;
}

/* Exit the character device by freeing up the resources that it created and
 * disconnecting itself from the kernel.
 */
static void cdevice_exit(struct dma_proxy_channel *pchannel_p, int last_channel)
{
	/* Take everything down in the reverse order
	 * from how it was created for the char device
	 */
	if (pchannel_p->proxy_device_p) {
		device_destroy(pchannel_p->class_p, pchannel_p->dev_node);
		if (last_channel)
			class_destroy(pchannel_p->class_p);

		cdev_del(&pchannel_p->cdev);
		unregister_chrdev_region(pchannel_p->dev_node, 1);
	}
}

/* Create a DMA channel by getting a DMA channel from the DMA Engine and then setting
 * up the channel as a character device to allow user space control.
 */
static int create_channel(struct platform_device *pdev, struct dma_proxy_channel *pchannel_p, char *name, u32 direction)
{
	int rc;
	//struct dma_chan *dchan;

	/* Request the DMA channel from the DMA engine and then use the device from
	 * the channel for the proxy channel also.
	 */
	pchannel_p->channel_p = dma_request_slave_channel(&pdev->dev, name);
	if (!pchannel_p->channel_p) {
		dev_err(pchannel_p->dma_device_p, "DMA channel request error\n");
		return ERROR;
	}
	pchannel_p->dma_device_p = &pdev->dev; 

	/* Initialize the character device for the dma proxy channel
	 */
	rc = cdevice_init(pchannel_p, name);
	if (rc) 
		return rc;

	pchannel_p->direction = direction;

	/* Allocate DMA memory for the proxy channel interface.
	 */
	pchannel_p->interface_p = (struct dma_proxy_channel_interface *)
		dmam_alloc_coherent(pchannel_p->dma_device_p,
					sizeof(struct dma_proxy_channel_interface),
					&pchannel_p->interface_phys_addr, GFP_KERNEL);
		printk(KERN_INFO "Allocating uncached memory at virtual address 0x%p, physical address 0x%p\n", 
			pchannel_p->interface_p, (void *)pchannel_p->interface_phys_addr);

	if (!pchannel_p->interface_p) {
		dev_err(pchannel_p->dma_device_p, "DMA allocation error\n");
		return ERROR;
	}
	
	dchan = dma_get_slave_channel(pchannel_p->channel_p);
	if (!dchan)
		pr_err("Error getting dma slave channel\n");
	pr_info("Got dma channel at address %p\n", dchan);
	pr_info("VDMA cfg address %p\n", vdma_cfg);
	rc = xilinx_vdma_channel_set_config(dchan, vdma_cfg);
	if (!rc)
		pr_info("VDMA channel set config success\n");
	else{
		pr_info("VDMA channel set config failed\n");
		return rc;
	}
	kfree(dchan);
	return 0;
}

/* Initialize the dma proxy device driver module.
 */
static int dma_proxy_probe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "dma_proxy module initialized\n");
	vdma_cfg = kmalloc(sizeof(struct xilinx_vdma_config *), GFP_KERNEL);

	if (!vdma_cfg) {
		pr_err("VDMA config struct is null\n");
		}else pr_info("VDMA config struct address : %p\n", vdma_cfg);
	memset((void *) vdma_cfg, 0, sizeof(struct xilinx_vdma_config *));// Zero config
	frm_cnt=3;// same as "xlnx,num-fstores"
	/*rc = of_property_read_u32(pdev->dev.of_node, "xlnx,num-fstores", &frm_cnt);
	if (rc) {
		pr_err("no xlnx,num-fstores property found\n");
		}*///Cannot get property like this since it was not included in device tree node in pl.dtsi
	
	vdma_cfg->frm_cnt_en=1;
	vdma_cfg->coalesc=frm_cnt*10;
	
	dchan = kmalloc(sizeof(struct dma_chan *), GFP_KERNEL);
	if (!dchan)
		pr_err("Error allocating dma slave channel\n");

	/* Create the transmit and receive channels.
	 */
	rc = create_channel(pdev, &channels[TX_CHANNEL], "dma_proxy_tx", DMA_MEM_TO_DEV);

	if (rc) 
		return rc;

	rc = create_channel(pdev, &channels[RX_CHANNEL], "dma_proxy_rx", DMA_DEV_TO_MEM);
	if (rc) 
		return rc;
	
	/*dchan = dma_get_slave_channel();	
	rc = xilinx_vdma_channel_set_config(channels[RX_CHANNEL].channel_p, vdma_cfg);
	if (!rc)
		pr_info("VDMA RX channel set config success\n");
	else 
		return rc;*/
	/*rc = xilinx_vdma_channel_set_config(channels[TX_CHANNEL].channel_p, vdma_cfg);
	if (!rc)
		pr_info("VDMA TX channel set config success\n");
	else 
		return rc;*/


	if (internal_test)
		test();
	kfree(vdma_cfg);
	return 0;
}
 
/* Exit the dma proxy device driver module.
 */
static int dma_proxy_remove(struct platform_device *pdev)
{
	int i;

	printk(KERN_INFO "dma_proxy module exited\n");

	/* Take care of the char device infrastructure for each
	 * channel except for the last channel. Handle the last
	 * channel seperately.
	 */
	for (i = 0; i < CHANNEL_COUNT - 1; i++) 
		if (channels[i].proxy_device_p)
			cdevice_exit(&channels[i], NOT_LAST_CHANNEL);

	cdevice_exit(&channels[i], LAST_CHANNEL);

	/* Take care of the DMA channels and the any buffers allocated
	 * for the DMA transfers. The DMA buffers are using managed
	 * memory such that it's automatically done.
	 */
	for (i = 0; i < CHANNEL_COUNT; i++)
		if (channels[i].channel_p) {
			channels[i].channel_p->device->device_terminate_all(channels[i].channel_p);
			dma_release_channel(channels[i].channel_p);
		}
	return 0;
}

static const struct of_device_id dma_proxy_of_ids[] = {
	{ .compatible = "xlnx,dma_proxy",},
	{}
};

static struct platform_driver dma_proxy_driver = {
	.driver = {
		.name = "dma_proxy_driver",
		.owner = THIS_MODULE,
		.of_match_table = dma_proxy_of_ids,
	},
	.probe = dma_proxy_probe,
	.remove = dma_proxy_remove,
};

static int __init dma_proxy_init(void)
{
	return platform_driver_register(&dma_proxy_driver);

}

static void __exit dma_proxy_exit(void)
{
	platform_driver_unregister(&dma_proxy_driver);
}

module_init(dma_proxy_init)
module_exit(dma_proxy_exit)

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("DMA Proxy Prototype");
MODULE_LICENSE("GPL v2");
