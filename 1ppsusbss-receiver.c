#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/pps_kernel.h>
#include <linux/slab.h>

#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x81
#define MAX_PKT_SIZE 512
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

#define SEND_DELAY_MAX		100000000

static unsigned int send_delay = 100000000;
MODULE_PARM_DESC(delay,
	"Delay between setting and dropping the signal (ns)");
module_param_named(delay, send_delay, uint, 0);


#define SAFETY_INTERVAL	3000	/* set the hrtimer earlier for safety (ns) */

struct pps_receiver_cyfx3 {
	struct usb_device *usbd;
	struct task_struct *task;
	struct pps_device *pps;
};

void recv_read_callback(struct urb *u)
{
	usb_free_urb(u);
  	return;
}

static int pps_recv_thread_callback(void *data)
{
	struct pps_receiver_cyfx3 *r = data;
	struct usb_device *usbd = r->usbd;
	struct pps_event_time ts_assert, ts_clear;
	int ret = 0, read_cnt = 0;
	unsigned char *buf;

	buf = kzalloc(sizeof(char) * MAX_PKT_SIZE, GFP_ATOMIC);
	if (!buf)
	{
		dev_err(&usbd->dev, "Cannot allocate usb buffer!\n");
		return -ENOMEM;
	}

	while(!kthread_should_stop()) {
		//dev_info(&usbd->dev, "In recv thread\n");
		ret = usb_bulk_msg(usbd, usb_rcvbulkpipe(usbd,
			      BULK_EP_IN), buf, MAX_PKT_SIZE, &read_cnt, 1500);
		if(ret == -ETIMEDOUT)
		{
			dev_err(&usbd->dev,
			    "CAPTUREASSERT timeout\n");
		}
		else if (ret !=0) {
			dev_err(&usbd->dev,
			    "CAPTUREASSERT missed with error %d\n", ret);
		}
		else {
			//dev_info(&usbd->dev, "get %d\n", buf[0]);
			if(buf[0] != 0) {
				pps_get_ts(&ts_assert);
				pps_event(r->pps, &ts_assert, PPS_CAPTUREASSERT,
			    					NULL);
			}
			else {
				dev_info(&usbd->dev, "Skip clear event\n");
			}
		}

		ret = 0;

		ret = usb_bulk_msg(usbd, usb_rcvbulkpipe(usbd,
			      BULK_EP_IN), buf, MAX_PKT_SIZE, &read_cnt, 300);
		if (ret == -ETIMEDOUT) {
			dev_err(&usbd->dev, "CAPTURECLEAR timeout\n");
		}
		else if(ret !=0) {
			dev_err(&usbd->dev, "CAPTURECLEAR missed with error %d\n",
			    ret);
		}
		else {
			//dev_info(&usbd->dev, "get %d\n", buf[0]);
			if (buf[0] == 0) {
				pps_get_ts(&ts_clear);
				pps_event(r->pps, &ts_clear, PPS_CAPTURECLEAR, NULL);
			}
			else {
				dev_info(&usbd->dev, "Skip event\n");
			}
		}
	}
	kfree(buf);
	return 0;
}


static int pps_cfx3_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *usbd = NULL;
	struct pps_receiver_cyfx3 *r = NULL;
	struct pps_source_info info = {
		.name = KBUILD_MODNAME,
		.path = "",
		.mode = PPS_CAPTUREBOTH | PPS_OFFSETASSERT | PPS_OFFSETCLEAR | 
		  	PPS_ECHOASSERT | PPS_ECHOCLEAR | PPS_CANWAIT |
			PPS_TSFMT_TSPEC,
		.owner = THIS_MODULE,
		.dev = NULL
	};

	usbd = interface_to_usbdev(interface);

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return -ENOMEM;

	r->pps = pps_register_source(&info, PPS_CAPTUREBOTH | PPS_OFFSETASSERT |
	    					PPS_OFFSETCLEAR);
	if (!r->pps) {
		dev_err(&usbd->dev, "Cannot register PPS source\n");
		return -EFAULT;
	}

	dev_info(&usbd->dev, "Run PPS task\n");
	r->usbd = usbd;
	dev_set_drvdata(&usbd->dev, r);
	r->task = kthread_create(pps_recv_thread_callback, r, "cyfx3-pps-recv");
	if (!r->task) {
		dev_err(&usbd->dev, "Unable to create recv thread\n");
		return -EAGAIN;
	}
	wake_up_process(r->task);

	dev_info(&usbd->dev, "Probe\n");

	return 0;
}
 
static void pps_cfx3_disconnect(struct usb_interface *interface)
{
  	struct usb_device *usbd = interface_to_usbdev(interface);
	struct pps_receiver_cyfx3 *r = dev_get_drvdata(&usbd->dev);
	if(kthread_stop(r->task)) {
		dev_err(&usbd->dev, "Error on recv task stop\n");
	}
	pps_unregister_source(r->pps);
	kfree(r);
	dev_info(&usbd->dev, "Disconnect\n");
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
  	int ret = 0;
	ret = usb_register(&pps_cfx3_driver);
	printk( KERN_INFO "usb_register: %d\n", ret);
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
