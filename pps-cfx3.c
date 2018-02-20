#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>

#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x81
#define MAX_PKT_SIZE 512
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

#define SEND_DELAY_MAX		100000

static unsigned int send_delay = 30000;
MODULE_PARM_DESC(delay,
	"Delay between setting and dropping the signal (ns)");
module_param_named(delay, send_delay, uint, 0);


#define SAFETY_INTERVAL	3000	/* set the hrtimer earlier for safety (ns) */

/* internal per port structure */
struct pps_generator_cyfx3 {
	struct usb_device *usbd;	/* USB device */
	struct hrtimer timer;
	int attached;

	/* calibrated time between a hrtimer event and the reaction */
	long hrtimer_error;

	long port_write_time;		/* calibrated port write time (ns) */
};

static int pps_write(struct usb_device *usbd, u8 value)
{
  	int ret = 0;
	int wrote_cnt = 0;
/*	ret = usb_bulk_msg(usbd, usb_sndbulkpipe(usbd, BULK_EP_OUT),
			&value, sizeof(value), &wrote_cnt, 5000);
	dev_info(&usbd->dev, "wr %d, %d\n", wrote_cnt, ret);*/
	return ret;
}

/* the kernel hrtimer event */
static enum hrtimer_restart hrtimer_event(struct hrtimer *timer)
{
	struct timespec expire_time, ts1, ts2, ts3, dts;
	struct pps_generator_cyfx3 *gen;
	struct usb_device *usbd;
	long lim, delta;
	unsigned long flags;

	/* We have to disable interrupts here. The idea is to prevent
	 * other interrupts on the same processor to introduce random
	 * lags while polling the clock. getnstimeofday() takes <1us on
	 * most machines while other interrupt handlers can take much
	 * more potentially.
	 *
	 * NB: approx time with blocked interrupts =
	 * send_delay + 3 * SAFETY_INTERVAL
	 */
	local_irq_save(flags);

	/* first of all we get the time stamp... */
	getnstimeofday(&ts1);
	expire_time = ktime_to_timespec(hrtimer_get_softexpires(timer));
	gen = container_of(timer, struct pps_generator_cyfx3, timer);
	usbd = gen->usbd;
	lim = NSEC_PER_SEC - send_delay - gen->port_write_time;

	/* check if we are late */
	if (expire_time.tv_sec != ts1.tv_sec || ts1.tv_nsec > lim) {
		local_irq_restore(flags);
		pr_err("we are late this time %ld.%09ld\n",
				ts1.tv_sec, ts1.tv_nsec);
		goto done;
	}

	/* busy loop until the time is right for an assert edge */
	do {
		getnstimeofday(&ts2);
	} while (expire_time.tv_sec == ts2.tv_sec && ts2.tv_nsec < lim);

	/* set the signal */
	//USB BULK WRITE 1
	pps_write(usbd, 1);

	/* busy loop until the time is right for a clear edge */
	lim = NSEC_PER_SEC - gen->port_write_time;
	do {
		getnstimeofday(&ts2);
	} while (expire_time.tv_sec == ts2.tv_sec && ts2.tv_nsec < lim);

	/* unset the signal */
	//USB BULK WRITE 0
	pps_write(usbd, 0);
	printk(KERN_CRIT "low\n");

	getnstimeofday(&ts3);

	local_irq_restore(flags);

	/* update calibrated port write time */
	dts = timespec_sub(ts3, ts2);
	gen->port_write_time =
		(gen->port_write_time + timespec_to_ns(&dts)) >> 1;

done:
	/* update calibrated hrtimer error */
	dts = timespec_sub(ts1, expire_time);
	delta = timespec_to_ns(&dts);
	/* If the new error value is bigger then the old, use the new
	 * value, if not then slowly move towards the new value. This
	 * way it should be safe in bad conditions and efficient in
	 * good conditions.
	 */
	if (delta >= gen->hrtimer_error)
		gen->hrtimer_error = delta;
	else
		gen->hrtimer_error = (3 * gen->hrtimer_error + delta) >> 2;

	/* update the hrtimer expire time */
	hrtimer_set_expires(timer,
			ktime_set(expire_time.tv_sec + 1,
				NSEC_PER_SEC - (send_delay +
				gen->port_write_time + SAFETY_INTERVAL +
				2 * gen->hrtimer_error)));

	return HRTIMER_RESTART;
}

/* calibrate port write time */
#define PORT_NTESTS_SHIFT	5
static void calibrate_port(struct pps_generator_cyfx3 *gen)
{
	struct usb_device *usbd = gen->usbd;
	int i;
	long acc = 0;

	for (i = 0; i < (1 << PORT_NTESTS_SHIFT); i++) {
		struct timespec a, b;
		unsigned long irq_flags;

		local_irq_save(irq_flags);
		getnstimeofday(&a);
		//port->ops->write_control(port, NO_SIGNAL);
		//USB BULK WRITE 0
		pps_write(usbd, 0);
		getnstimeofday(&b);
		local_irq_restore(irq_flags);

		b = timespec_sub(b, a);
		acc += timespec_to_ns(&b);
	}

	gen->port_write_time = acc >> PORT_NTESTS_SHIFT;
	pr_info("port write takes %ldns\n", gen->port_write_time);
}

static inline ktime_t next_intr_time(struct pps_generator_cyfx3 *gen)
{
	struct timespec ts;

	getnstimeofday(&ts);

	return ktime_set(ts.tv_sec +
			((ts.tv_nsec > 990 * NSEC_PER_MSEC) ? 1 : 0),
			NSEC_PER_SEC - (send_delay +
			gen->port_write_time + 3 * SAFETY_INTERVAL));
}

static int pps_cfx3_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *usbd = NULL;
	struct pps_generator_cyfx3 *gen = NULL;

	usbd = interface_to_usbdev(interface);

	gen = kzalloc(sizeof(*gen), GFP_KERNEL);
	if (!gen)
		return -ENOMEM;
	dev_info(&usbd->dev, "allocate generator at 0x%p\n", gen);

	gen->usbd = usbd;
	dev_set_drvdata(&usbd->dev, gen);
	
	gen->hrtimer_error = SAFETY_INTERVAL;
	dev_info(&usbd->dev, "attached to usb device %d\n", usbd->devnum);
	gen->attached = 1;

	calibrate_port(gen);

	hrtimer_init(&gen->timer, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	gen->timer.function = hrtimer_event;
	hrtimer_start(&gen->timer, next_intr_time(gen), HRTIMER_MODE_ABS);
	
	return 0;
}
 
static void pps_cfx3_disconnect(struct usb_interface *interface)
{
  	struct usb_device *usbd = interface_to_usbdev(interface);
	struct pps_generator_cyfx3 *gen = dev_get_drvdata(&usbd->dev);
	hrtimer_cancel(&gen->timer);
	dev_info(&usbd->dev, "free generator at 0x%p\n", gen);
	kfree(gen);
  	//usb_deregister_dev(interface, &class);
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
