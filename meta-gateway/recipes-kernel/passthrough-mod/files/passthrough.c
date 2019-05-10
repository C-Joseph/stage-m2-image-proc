/*
 * Passthrough kernel module driver for passthrough IP.
 *
 * Copyright 2008 - 2013 Xilinx, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#define DEBUG

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
//#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/fs.h>//For file operations structure
#include <linux/cdev.h>

//---------------From xpassthrough_hw.h generated by Xilinx------------------
// CONTROL_BUS
// 0x0 : Control signals
//       bit 0  - ap_start (Read/Write/COH)
//       bit 1  - ap_done (Read/COR)
//       bit 2  - ap_idle (Read)
//       bit 3  - ap_ready (Read)
//       bit 7  - auto_restart (Read/Write)
//       others - reserved
// 0x4 : Global Interrupt Enable Register
//       bit 0  - Global Interrupt Enable (Read/Write)
//       others - reserved
// 0x8 : IP Interrupt Enable Register (Read/Write)
//       bit 0  - Channel 0 (ap_done)
//       bit 1  - Channel 1 (ap_ready)
//       others - reserved
// 0xc : IP Interrupt Status Register (Read/TOW)
//       bit 0  - Channel 0 (ap_done)
//       bit 1  - Channel 1 (ap_ready)
//       others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

//Offsets for Passthrough HLS IP 
#define PASSTHROUGH_CONTROL_BUS_ADDR_AP_CTRL	0x0
#define PASSTHROUGH_CONTROL_BUS_ADDR_GIE	0x4
#define PASSTHROUGH_CONTROL_BUS_ADDR_IER	0x8
#define PASSTHROUGH_CONTROL_BUS_ADDR_ISR	0xc

#define PASSTHROUGH_DEVICE_ID	0
//#define PASSTHROUGH_CONTROL_BUS_BASE_ADDR	0x43C00000
//---------------------------------------------------------------------
//---------------From xil_types.h generated by Xilinx------------------
#define XIL_COMPONENT_IS_READY 0x11111111U
//#define XIL_COMPONENT_IS_STARTED 0x22222222U
//---------------------------------------------------------------------
/* Read/Write access to IP registers */
#if defined(CONFIG_ARCH_ZYNQ) || defined(CONFIG_ARM64)
# define passthrough_readreg(offset)		readl(offset)
# define passthrough_writereg(offset, val)	writel(val, offset)
#else
# define passthrough_readreg(offset)		__raw_readl(offset)
# define passthrough_writereg(offset, val)	__raw_writel(val, offset)
#endif

#define DRIVER_NAME "passthrough_driver"


struct passthrough_instance {
	u16 DeviceId;
	u32 ctrl_bus_base_address;
	u32 ctrl_bus_bus_addr_width;
	u32 ctrl_bus_bus_data_width;
	u32 IsReady;
	//Character device members
	dev_t dev_node;
	struct device *passthrough_device;
	struct cdev cdev;
	struct class *class_passthrough;
	//--------------------------------
	struct clk *clk;
};
//---------------------------------------------------------------------------------------
//-------Character device functions

static int passthrough_local_open(struct inode *ino, struct file *file)
{
	file->private_data = container_of(ino->i_cdev, struct passthrough_instance, cdev);
	printk("Device open\n");
	return 0;
}
static int passthrough_release(struct inode *ino, struct file *file)
{
	printk("Device closed\n");
	return 0;
}

static ssize_t passthrough_write(struct file *fileptr, const char *buf, size_t count, loff_t *f_pos){
	printk("Writing to device ...\n");
	return 1;
}

static ssize_t passthrough_read(struct file *fileptr, char *buf, size_t count, loff_t *f_pos){
	printk("Reading from device ...\n");
	return 1;
}

static long passthrough_ioctl(struct file *file, unsigned int unused , unsigned long arg)
{
	printk("IOCTL call\n");
	return 0;
}
static struct file_operations passthrough_fops = {
	.owner    = THIS_MODULE,
	.open     = passthrough_local_open,
	.read     = passthrough_read,
	.write     = passthrough_write,
	.release  = passthrough_release,
	.unlocked_ioctl = passthrough_ioctl,
};

//static struct class *class_passthrough;

static int cdevice_init(struct passthrough_instance *HLS_IP)
{
	int status;
	char device_name[32] = "passthrough";
	struct device *subdev;
	
	status=0;

	/* Allocate a character device from the kernel for this driver.
	 */
	status = alloc_chrdev_region(&HLS_IP->dev_node, 0, 1, "passthrough");

	if (status) {
		dev_err(HLS_IP->passthrough_device, "unable to get a char device number\n");
		//pr_err("unable to get a char device number\n");
		return status;
	}

	/* Initialize the device data structure before registering the character 
	 * device with the kernel.
	 */
	cdev_init(&HLS_IP->cdev, &passthrough_fops);
	HLS_IP->cdev.owner = THIS_MODULE;
	status = cdev_add(&HLS_IP->cdev, HLS_IP->dev_node, 1);

	if (status) {
		dev_err(HLS_IP->passthrough_device, "unable to add char device\n");
		goto init_error1;
	}
	
	pr_info("Creating passthrough class...\n");
	/* Only one class in sysfs is to be created for multiple channels,
	 * create the device in sysfs which will allow the device node
	 * in /dev to be created
	 */
	HLS_IP->class_passthrough = class_create(THIS_MODULE, DRIVER_NAME);
	pr_info("class_create function called...\n");

	if (IS_ERR(HLS_IP->class_passthrough)) {
		dev_err(HLS_IP->passthrough_device, "unable to create class\n");
		status = -1;
		goto init_error2;
	}
	pr_info("Initialising passthrough class...\n");

	/* Create the device node in /dev so the device is accessible
	 * as a character device
	 */
	//strcat(device_name, name);
	//struct device *device_create(struct class *class, struct device *parent, dev_t devt, void *drvdata, const char *fmt, ...)
	subdev = device_create(HLS_IP->class_passthrough, HLS_IP->passthrough_device,
					  	 HLS_IP->dev_node, NULL, device_name);

	if (IS_ERR(subdev)) {
		dev_err(HLS_IP->passthrough_device, "unable to create the device\n");
		goto init_error3;
	}

	return 0;

init_error3:
	class_destroy(HLS_IP->class_passthrough);

init_error2:
	cdev_del(&HLS_IP->cdev);

init_error1:
	unregister_chrdev_region(HLS_IP->dev_node, 1);
	return status;
}

/* Exit the character device by freeing up the resources that it created and
 * disconnecting itself from the kernel.
 */
static void cdevice_exit(struct passthrough_instance *HLS_IP)
{
	/* Take everything down in the reverse order
	 * from how it was created for the char device
	 */
	device_destroy(HLS_IP->class_passthrough, HLS_IP->dev_node);
	class_destroy(HLS_IP->class_passthrough);

	cdev_del(&HLS_IP->cdev);
	unregister_chrdev_region(HLS_IP->dev_node, 1);
}
//---------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------
//-------Platform device functions
static irqreturn_t passthrough_irqhandler(int irq, void *dev_id)
{
	pr_info("Interrupt triggered!\n");
	return IRQ_HANDLED;
}

static int passthrough_remove(struct platform_device *pdev)
{
	struct passthrough_instance *HLS_IP;
	pm_runtime_disable(&pdev->dev);
	HLS_IP = platform_get_drvdata(pdev);
	cdevice_exit(HLS_IP);
	printk("Removing...\n");
	return 0;
}

static int passthrough_probe(struct platform_device *pdev)
{
	int irq; u32 tmp;
	struct device_node *np;
	struct passthrough_instance *HLS_IP;
	int status;
	struct resource *res;

	printk("Probing...\n");
	np = pdev->dev.of_node;
	status = 0;

	//Driver data memory allocation-----------------------
	HLS_IP = devm_kzalloc(&pdev->dev, sizeof(*HLS_IP), GFP_KERNEL);
	//devm_kzalloc() initialises allocated memory with zeros, memory freed automatically
	if (!HLS_IP){
		dev_err(&pdev->dev, "Could not allocate memory for passthrough device\n");
		return -ENOMEM;
	}
	else printk("Memory allocated for HLS IP at 0x%p\n", HLS_IP);
	//----------------------Set device data------------------------------
	of_property_read_u32(np, "xlnx,s-axi-control-bus-addr-width", &HLS_IP->ctrl_bus_bus_addr_width);
	of_property_read_u32(np, "xlnx,s-axi-control-bus-data-width", &HLS_IP->ctrl_bus_bus_data_width);
	HLS_IP->DeviceId = 0;
	HLS_IP->IsReady = XIL_COMPONENT_IS_READY;
	
	platform_set_drvdata(pdev, HLS_IP);//Sets driver data
	
	HLS_IP->clk = devm_clk_get(&pdev->dev, "ap_clk");
	if (IS_ERR(HLS_IP->clk)) {
		if (PTR_ERR(HLS_IP->clk) != -ENOENT) {
			dev_err(&pdev->dev, "Input clock not found\n");
			return PTR_ERR(HLS_IP->clk);
		}

		/*
		 * Clock framework support is optional, continue on
		 * anyways if we don't find a matching clock.
		 */
		HLS_IP->clk = NULL;
	}

	status = clk_prepare(HLS_IP->clk);
	if (status < 0) {
		dev_err(&pdev->dev, "Failed to preapre clk\n");
		return status;
	}
	pm_runtime_enable(&pdev->dev);
	status = pm_runtime_get_sync(&pdev->dev);
	if (status < 0)
		goto err_unprepare_clk;
	//Set up memory to allow device access, map physical memory into virtual address space
	printk("Mapping physical memory ...\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	HLS_IP->ctrl_bus_base_address = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(HLS_IP->ctrl_bus_base_address)){
		dev_err(&pdev->dev, "Could not map physical memory  to virtual memory for passthrough device\n");
		return PTR_ERR(HLS_IP->ctrl_bus_base_address);
	}
	else printk("Physical memory at 0x%p mapped to 0x%p\n", (void *)res->start, (void *)HLS_IP->ctrl_bus_base_address);

	//------------Interrupts------------------------
	irq = platform_get_irq(pdev, 0);
	pr_alert("Interrupt number: %d\n", irq);
	printk("Device name: %s\n", dev_name(&pdev->dev));
	
	//devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
	status= devm_request_irq(&pdev->dev, irq, &passthrough_irqhandler, IRQF_SHARED, dev_name(&pdev->dev), HLS_IP);
	if (status) {
		pr_err("%s: Passthrough HLS IP IRQ registration failed %d\n",
		       np->full_name, status);
		goto err_pm_put;
	}	

	pr_info("Passthrough: %s: registered\n", np->full_name);
	//----------Starting device------------------------

	pr_info("Enabling auto restart...\n");
	passthrough_writereg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_AP_CTRL, 0x80);

	pr_info("Enabling global interrupt...\n");
	passthrough_writereg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_GIE, 0x1);
	pr_info("Starting device...\n");
	tmp=(passthrough_readreg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_AP_CTRL)) & 0x80;//Ensure auto restart
	passthrough_writereg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_AP_CTRL, tmp|0x1);//set ap_start=1
	//Read interrupt registers
	tmp=passthrough_readreg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_IER);
	pr_info("IER value: %d\n",tmp);
	tmp=passthrough_readreg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_GIE);
	pr_info("GIE value: %d\n",tmp);
	//Read CONTROL_BUS value
	tmp = passthrough_readreg(HLS_IP->ctrl_bus_base_address + PASSTHROUGH_CONTROL_BUS_ADDR_AP_CTRL);
	pr_info("Ctrl Reg = %d\n",tmp);
	//-------------------------------
	status = cdevice_init(HLS_IP);
	if (status) {
		pr_err("Error at char device init\n");
		return status;
	}

	pm_runtime_put(&pdev->dev);
	return 0;

err_pm_put:
	pm_runtime_put(&pdev->dev);
err_unprepare_clk:
	pm_runtime_disable(&pdev->dev);
	clk_unprepare(HLS_IP->clk);
	return status;
}

static const struct of_device_id passthrough_of_match[] = {
	{ .compatible = "xlnx,passthrough-1.0", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, passthrough_of_match);

static struct platform_driver passthrough_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = passthrough_of_match,
	},
	.probe = passthrough_probe,
	.remove = passthrough_remove,
};

static int __init passthrough_init(void)
{	printk("Initialising module\n");
	return(platform_driver_register(&passthrough_driver));
}

static void __exit passthrough_exit(void)
{
	printk("Exiting module\n");
	platform_driver_unregister(&passthrough_driver);
}
//---------------------------------------------------------------------------------------
module_init(passthrough_init);
module_exit(passthrough_exit);

MODULE_AUTHOR("Cleo");
MODULE_DESCRIPTION("Passthrough HLS IP driver");
MODULE_LICENSE("GPL");
