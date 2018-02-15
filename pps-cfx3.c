#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/uaccess.h>

#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x81
#define MAX_PKT_SIZE 512
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

static struct usb_device *device;
static struct usb_class_driver class;
//static unsigned char bulk_buf[MAX_PKT_SIZE];

static int pps_cfx3_open(struct inode *i, struct file *f)
{
  	printk("PPS open was called\n");
    return 0;
}
static int pps_cfx3_close(struct inode *i, struct file *f)
{
  	printk("PPS close was called\n");
    return 0;
}
static ssize_t pps_cfx3_read(struct file *f, char __user *buf, size_t cnt, loff_t *off)
{
  	int retval;
	int read_cnt;
	static unsigned char *bulk_buf;

	bulk_buf = kzalloc(sizeof(char) * MAX_PKT_SIZE, GFP_KERNEL);
	if (!bulk_buf) {
	  	printk("cfx3 read: cannot allocate buf\n");
		return -ENOMEM;
	}

	/* Read the data from the bulk endpoint */
	retval = usb_bulk_msg(device, usb_rcvbulkpipe(device, BULK_EP_IN),
			bulk_buf, MAX_PKT_SIZE, &read_cnt, 5000);
	if (retval)
	{
		printk(KERN_ERR "Bulk message returned %d\n", retval);
		goto err;
	}
	printk("PPS read bulk_buf[0] = 0x%x, cnt = %d\n", bulk_buf[0], read_cnt);
	if (copy_to_user(buf, bulk_buf, MIN(cnt, read_cnt)))
	{
		retval = -EFAULT;
		goto err;
	}

  	printk("PPS read was called: %lu bytes\n", cnt);
	kfree(bulk_buf);
	return MIN(cnt, read_cnt);

err:
	kfree(bulk_buf);
	return retval;

}

static ssize_t pps_cfx3_write(struct file *f, const char __user *buf, size_t cnt, loff_t *off)
{
	int retval;
	int wrote_cnt = MIN(cnt, MAX_PKT_SIZE);
	static unsigned char *bulk_buf;

	bulk_buf = kzalloc(sizeof(char) * MAX_PKT_SIZE, GFP_KERNEL);
	if (!bulk_buf) {
	  	printk("cfx3 write: cannot allocate buf\n");
		return -ENOMEM;
	}

	if (copy_from_user(bulk_buf, buf, MIN(cnt, MAX_PKT_SIZE)))
	{
		retval = -EFAULT;
		goto err;
	}
	printk("PPS write bulk_buf[0] = 0x%x, cnt = %d\n", bulk_buf[0], wrote_cnt);

	/* Write the data into the bulk endpoint */
	retval = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT),
			bulk_buf, MIN(cnt, MAX_PKT_SIZE), &wrote_cnt, 5000);
	if (retval)
	{
		printk(KERN_ERR "Bulk message returned %d\n", retval);
		goto err;;
	}

  	printk("PPS write was called: %lu bytes\n", cnt);
	kfree(bulk_buf);
	return wrote_cnt;

err:
	kfree(bulk_buf);
	return retval;
}

static struct file_operations fops =
{
    .open = pps_cfx3_open,
    .release = pps_cfx3_close,
    .read = pps_cfx3_read,
    .write = pps_cfx3_write,
};

//static int send_byte(unsigned char b)
//{
//  	int retval;
//	int wrote_cnt = MAX_PKT_SIZE;
//
//	memset(bulk_buf, 0, MAX_PKT_SIZE);
//	bulk_buf[0] = b;
//	bulk_buf[1] = b;
//	bulk_buf[2] = b;
//	bulk_buf[3] = b;
//	printk(KERN_INFO "Write byte 0x%x\n", bulk_buf[0]);
//	retval = usb_bulk_msg(device, usb_sndbulkpipe(device, BULK_EP_OUT),
//	    bulk_buf, MAX_PKT_SIZE, &wrote_cnt, 3000);
//	if (retval)
//	{
//	  	printk( KERN_INFO "Send retval is %d\n", retval);
//	}
//	printk(KERN_INFO "wrote_cnt is %d\n", wrote_cnt);
//
//	return 0;	
//}

//static int recv_byte(void)
//{
//  	int retval;
//	int read_cnt = MAX_PKT_SIZE;
//	int tmp = 0xcccccccc;
//
//	retval = usb_bulk_msg(device, usb_rcvbulkpipe(device, BULK_EP_IN),
//	    &tmp, MAX_PKT_SIZE, &read_cnt, 30000);
//	if (retval)
//	{
//	  	printk( KERN_ERR "Recv retval is %d\n", retval);
//	}
//	printk(KERN_INFO "read_cnt is %d\n", read_cnt);
//
//	return tmp;	
//}

static int pps_cfx3_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
  	int retval, tmp;

	device = interface_to_usbdev(interface);
	class.name = "usb/pps_cfx3%d";
	class.fops = &fops;
	if ((retval = usb_register_dev(interface, &class)) < 0)
	{
        	/* Something prevented us from registering this driver */
        	printk(KERN_INFO "Not able to get a minor for this device.");
	}
	else
	{
		printk(KERN_INFO "Minor obtained: %d\n", interface->minor);
	}
	return retval;
}
 
static void pps_cfx3_disconnect(struct usb_interface *interface)
{
  	usb_deregister_dev(interface, &class);
	printk(KERN_INFO "Cfx3 pps drive removed\n");
}
 
static struct usb_device_id pps_cfx3_table[] =
{
    { USB_DEVICE(0x04b4, 0x00f0) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pps_cfx3_table);

static struct usb_driver pps_cfx3_driver =
{
	.name = "cfx3_pps_driver",
	.id_table = pps_cfx3_table,
	.probe = pps_cfx3_probe,
	.disconnect = pps_cfx3_disconnect,
};

static int __init pps_cfx3_init(void)
{
  	int tmp = 0, ret = 0;
	ret = usb_register(&pps_cfx3_driver);
	printk( KERN_INFO "usb_register: %d\n", ret);
	/* Do test blink 
	msleep(2000);
	send_byte(0);
	msleep(1000);
	send_byte(0xff);
	msleep(1000);
	send_byte(0);
	msleep(1000);
	send_byte(0xff);
	msleep(1000);
	tmp = recv_byte();
	printk(KERN_INFO "Recv 0x%x\n", tmp);*/
	return ret;
}

static void __exit pps_cfx3_exit(void)
{
      usb_deregister(&pps_cfx3_driver);
}

module_init(pps_cfx3_init);
module_exit(pps_cfx3_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladimir Georgiev");
MODULE_AUTHOR("Alexander Gordeev <lasaine@lvk.cs.msu.su>");
MODULE_DESCRIPTION("Xypress FX3-based PPS board USB driver");
