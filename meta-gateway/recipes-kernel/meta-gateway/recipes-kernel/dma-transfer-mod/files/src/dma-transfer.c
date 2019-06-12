/*
 * XILINX AXI DMA Engine test module
 *
 * Copyright (C) 2010 Xilinx, Inc. All rights reserved.
 *
 * Based on Atmel DMA Test Client
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_dma.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/wait.h>
//#include <linux/sched/task.h>
#include <linux/dma/xilinx_dma.h>

extern int xilinx_vdma_channel_set_config(struct dma_chan *dchan, struct xilinx_vdma_config *cfg); //This function exported  to kernel in xilinx_dma.c

struct vdmatest_instance{
	
};

static int vdmatest_probe(struct platform_device *pdev)
{
	struct dma_chan *chan, *rx_chan;
	int err;
	//np = pdev->dev.of_node;
	//status = 0;
	
	pr_info("Probing of vdma test driver\n");
	//Is it necessary to reset VDMA values xilin-vdma has been probed??
	//I think so because 
	//Driver data memory allocation-----------------------
	/*HLS_IP = devm_kzalloc(&pdev->dev, sizeof(*HLS_IP), GFP_KERNEL);
	devm_kzalloc() initialises allocated memory with zeros, memory freed automatically
	if (!HLS_IP){
		dev_err(&pdev->dev, "Could not allocate memory for VDMA test device\n");
		return -ENOMEM;
	}
	else printk("Memory allocated for VDMA test device at 0x%p\n", HLS_IP);*/
	chan = dma_request_slave_channel(&pdev->dev, "vdma0");
	if (IS_ERR(chan)) {
		pr_err("xilinx_dmatest: No Tx channel\n");
		return PTR_ERR(chan);
	}

	rx_chan = dma_request_slave_channel(&pdev->dev, "vdma1");
	if (IS_ERR(rx_chan)) {
		err = PTR_ERR(rx_chan);
		pr_err("vdmatest: No Rx channel\n");
		goto free_tx;
	}

	//err = dmatest_add_slave_channels(chan, rx_chan);
	/*if (err) {
		pr_err("xilinx_dmatest: Unable to add channels\n");
		goto free_rx;
	}*/

	return 0;

free_rx:
	dma_release_channel(rx_chan);
free_tx:
	dma_release_channel(chan);

	return err;
}

static int vdmatest_remove(struct platform_device *pdev)
{
	//struct dmatest_chan *dtc, *_dtc;
	struct dma_chan *chan;
	pr_info("Removal of vdma test driver\n");
	/*list_for_each_entry_safe(dtc, _dtc, &dmatest_channels, node) {
		list_del(&dtc->node);
		chan = dtc->chan;
		dmatest_cleanup_channel(dtc);
		pr_info("xilinx_dmatest: dropped channel %s\n",
			dma_chan_name(chan));
		dmaengine_terminate_all(chan);
		dma_release_channel(chan);
	}*/
	dmaengine_terminate_all(chan);
	dma_release_channel(chan);
	return 0;
}

static const struct of_device_id vdmatest_of_ids[] = {
	{ .compatible = "xlnx,dma_proxy",},
	{}
};

static struct platform_driver vdmatest_driver = {
	.driver = {
		.name = "vdmatest",
		.owner = THIS_MODULE,
		.of_match_table = vdmatest_of_ids,
	},
	.probe = vdmatest_probe,
	.remove = vdmatest_remove,
};

static int __init vdma_init(void)
{
	pr_info("Initialisation of vdma test driver\n");
	return platform_driver_register(&vdmatest_driver);

}
late_initcall(vdma_init);

static void __exit vdma_exit(void)
{
	pr_info("Exit of vdma test driver\n");
	platform_driver_unregister(&vdmatest_driver);
}
module_exit(vdma_exit)

MODULE_AUTHOR("Cleo Joseph");
MODULE_DESCRIPTION("VDMA Test Module");
MODULE_LICENSE("GPL v2");

