
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/usb/input.h>

#define POWERMATE_VENDOR	0x077d	
#define POWERMATE_PRODUCT_NEW	0x0410	
#define POWERMATE_PRODUCT_OLD	0x04AA	

#define CONTOUR_VENDOR		0x05f3	
#define CONTOUR_JOG		0x0240	

#define SET_STATIC_BRIGHTNESS  0x01
#define SET_PULSE_ASLEEP       0x02
#define SET_PULSE_AWAKE        0x03
#define SET_PULSE_MODE         0x04

#define UPDATE_STATIC_BRIGHTNESS (1<<0)
#define UPDATE_PULSE_ASLEEP      (1<<1)
#define UPDATE_PULSE_AWAKE       (1<<2)
#define UPDATE_PULSE_MODE        (1<<3)

#define POWERMATE_PAYLOAD_SIZE_MAX 6
#define POWERMATE_PAYLOAD_SIZE_MIN 3
struct powermate_device {
	signed char *data;
	dma_addr_t data_dma;
	struct urb *irq, *config;
	struct usb_ctrlrequest *configcr;
	struct usb_device *udev;
	struct input_dev *input;
	spinlock_t lock;
	int static_brightness;
	int pulse_speed;
	int pulse_table;
	int pulse_asleep;
	int pulse_awake;
	int requires_update; 
	char phys[64];
};

static char pm_name_powermate[] = "Griffin PowerMate";
static char pm_name_soundknob[] = "Griffin SoundKnob";

static void powermate_config_complete(struct urb *urb);

static void powermate_irq(struct urb *urb)
{
	struct powermate_device *pm = urb->context;
	int retval;

	switch (urb->status) {
	case 0:
		
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		
		dbg("%s - urb shutting down with status: %d", __func__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d", __func__, urb->status);
		goto exit;
	}

	
	input_report_key(pm->input, BTN_0, pm->data[0] & 0x01);
	input_report_rel(pm->input, REL_DIAL, pm->data[1]);
	input_sync(pm->input);

exit:
	retval = usb_submit_urb (urb, GFP_ATOMIC);
	if (retval)
		err ("%s - usb_submit_urb failed with result %d",
		     __func__, retval);
}

static void powermate_sync_state(struct powermate_device *pm)
{
	if (pm->requires_update == 0)
		return; 
	if (pm->config->status == -EINPROGRESS)
		return; 

	if (pm->requires_update & UPDATE_PULSE_ASLEEP){
		pm->configcr->wValue = cpu_to_le16( SET_PULSE_ASLEEP );
		pm->configcr->wIndex = cpu_to_le16( pm->pulse_asleep ? 1 : 0 );
		pm->requires_update &= ~UPDATE_PULSE_ASLEEP;
	}else if (pm->requires_update & UPDATE_PULSE_AWAKE){
		pm->configcr->wValue = cpu_to_le16( SET_PULSE_AWAKE );
		pm->configcr->wIndex = cpu_to_le16( pm->pulse_awake ? 1 : 0 );
		pm->requires_update &= ~UPDATE_PULSE_AWAKE;
	}else if (pm->requires_update & UPDATE_PULSE_MODE){
		int op, arg;
		if (pm->pulse_speed < 255) {
			op = 0;                   
			arg = 255 - pm->pulse_speed;
		} else if (pm->pulse_speed > 255) {
			op = 2;                   
			arg = pm->pulse_speed - 255;
		} else {
			op = 1;                   
			arg = 0;                  
		}
		pm->configcr->wValue = cpu_to_le16( (pm->pulse_table << 8) | SET_PULSE_MODE );
		pm->configcr->wIndex = cpu_to_le16( (arg << 8) | op );
		pm->requires_update &= ~UPDATE_PULSE_MODE;
	} else if (pm->requires_update & UPDATE_STATIC_BRIGHTNESS) {
		pm->configcr->wValue = cpu_to_le16( SET_STATIC_BRIGHTNESS );
		pm->configcr->wIndex = cpu_to_le16( pm->static_brightness );
		pm->requires_update &= ~UPDATE_STATIC_BRIGHTNESS;
	} else {
		printk(KERN_ERR "powermate: unknown update required");
		pm->requires_update = 0; 
		return;
	}


	pm->configcr->bRequestType = 0x41; 
	pm->configcr->bRequest = 0x01;
	pm->configcr->wLength = 0;

	usb_fill_control_urb(pm->config, pm->udev, usb_sndctrlpipe(pm->udev, 0),
			     (void *) pm->configcr, NULL, 0,
			     powermate_config_complete, pm);

	if (usb_submit_urb(pm->config, GFP_ATOMIC))
		printk(KERN_ERR "powermate: usb_submit_urb(config) failed");
}

static void powermate_config_complete(struct urb *urb)
{
	struct powermate_device *pm = urb->context;
	unsigned long flags;

	if (urb->status)
		printk(KERN_ERR "powermate: config urb returned %d\n", urb->status);

	spin_lock_irqsave(&pm->lock, flags);
	powermate_sync_state(pm);
	spin_unlock_irqrestore(&pm->lock, flags);
}

static void powermate_pulse_led(struct powermate_device *pm, int static_brightness, int pulse_speed,
				int pulse_table, int pulse_asleep, int pulse_awake)
{
	unsigned long flags;

	if (pulse_speed < 0)
		pulse_speed = 0;
	if (pulse_table < 0)
		pulse_table = 0;
	if (pulse_speed > 510)
		pulse_speed = 510;
	if (pulse_table > 2)
		pulse_table = 2;

	pulse_asleep = !!pulse_asleep;
	pulse_awake = !!pulse_awake;


	spin_lock_irqsave(&pm->lock, flags);

	
	if (static_brightness != pm->static_brightness) {
		pm->static_brightness = static_brightness;
		pm->requires_update |= UPDATE_STATIC_BRIGHTNESS;
	}
	if (pulse_asleep != pm->pulse_asleep) {
		pm->pulse_asleep = pulse_asleep;
		pm->requires_update |= (UPDATE_PULSE_ASLEEP | UPDATE_STATIC_BRIGHTNESS);
	}
	if (pulse_awake != pm->pulse_awake) {
		pm->pulse_awake = pulse_awake;
		pm->requires_update |= (UPDATE_PULSE_AWAKE | UPDATE_STATIC_BRIGHTNESS);
	}
	if (pulse_speed != pm->pulse_speed || pulse_table != pm->pulse_table) {
		pm->pulse_speed = pulse_speed;
		pm->pulse_table = pulse_table;
		pm->requires_update |= UPDATE_PULSE_MODE;
	}

	powermate_sync_state(pm);

	spin_unlock_irqrestore(&pm->lock, flags);
}

static int powermate_input_event(struct input_dev *dev, unsigned int type, unsigned int code, int _value)
{
	unsigned int command = (unsigned int)_value;
	struct powermate_device *pm = input_get_drvdata(dev);

	if (type == EV_MSC && code == MSC_PULSELED){
		int static_brightness = command & 0xFF;   
		int pulse_speed = (command >> 8) & 0x1FF; 
		int pulse_table = (command >> 17) & 0x3;  
		int pulse_asleep = (command >> 19) & 0x1; 
		int pulse_awake  = (command >> 20) & 0x1; 

		powermate_pulse_led(pm, static_brightness, pulse_speed, pulse_table, pulse_asleep, pulse_awake);
	}

	return 0;
}

static int powermate_alloc_buffers(struct usb_device *udev, struct powermate_device *pm)
{
	pm->data = usb_alloc_coherent(udev, POWERMATE_PAYLOAD_SIZE_MAX,
				      GFP_ATOMIC, &pm->data_dma);
	if (!pm->data)
		return -1;

	pm->configcr = kmalloc(sizeof(*(pm->configcr)), GFP_KERNEL);
	if (!pm->configcr)
		return -ENOMEM;

	return 0;
}

static void powermate_free_buffers(struct usb_device *udev, struct powermate_device *pm)
{
	usb_free_coherent(udev, POWERMATE_PAYLOAD_SIZE_MAX,
			  pm->data, pm->data_dma);
	kfree(pm->configcr);
}

static int powermate_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev (intf);
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct powermate_device *pm;
	struct input_dev *input_dev;
	int pipe, maxp;
	int error = -ENOMEM;

	interface = intf->cur_altsetting;
	endpoint = &interface->endpoint[0].desc;
	if (!usb_endpoint_is_int_in(endpoint))
		return -EIO;

	usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
		0x0a, USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		0, interface->desc.bInterfaceNumber, NULL, 0,
		USB_CTRL_SET_TIMEOUT);

	pm = kzalloc(sizeof(struct powermate_device), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!pm || !input_dev)
		goto fail1;

	if (powermate_alloc_buffers(udev, pm))
		goto fail2;

	pm->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!pm->irq)
		goto fail2;

	pm->config = usb_alloc_urb(0, GFP_KERNEL);
	if (!pm->config)
		goto fail3;

	pm->udev = udev;
	pm->input = input_dev;

	usb_make_path(udev, pm->phys, sizeof(pm->phys));
	strlcat(pm->phys, "/input0", sizeof(pm->phys));

	spin_lock_init(&pm->lock);

	switch (le16_to_cpu(udev->descriptor.idProduct)) {
	case POWERMATE_PRODUCT_NEW:
		input_dev->name = pm_name_powermate;
		break;
	case POWERMATE_PRODUCT_OLD:
		input_dev->name = pm_name_soundknob;
		break;
	default:
		input_dev->name = pm_name_soundknob;
		printk(KERN_WARNING "powermate: unknown product id %04x\n",
		       le16_to_cpu(udev->descriptor.idProduct));
	}

	input_dev->phys = pm->phys;
	usb_to_input_id(udev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	input_set_drvdata(input_dev, pm);

	input_dev->event = powermate_input_event;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL) |
		BIT_MASK(EV_MSC);
	input_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	input_dev->relbit[BIT_WORD(REL_DIAL)] = BIT_MASK(REL_DIAL);
	input_dev->mscbit[BIT_WORD(MSC_PULSELED)] = BIT_MASK(MSC_PULSELED);

	
	pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);
	maxp = usb_maxpacket(udev, pipe, usb_pipeout(pipe));

	if (maxp < POWERMATE_PAYLOAD_SIZE_MIN || maxp > POWERMATE_PAYLOAD_SIZE_MAX) {
		printk(KERN_WARNING "powermate: Expected payload of %d--%d bytes, found %d bytes!\n",
			POWERMATE_PAYLOAD_SIZE_MIN, POWERMATE_PAYLOAD_SIZE_MAX, maxp);
		maxp = POWERMATE_PAYLOAD_SIZE_MAX;
	}

	usb_fill_int_urb(pm->irq, udev, pipe, pm->data,
			 maxp, powermate_irq,
			 pm, endpoint->bInterval);
	pm->irq->transfer_dma = pm->data_dma;
	pm->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	
	if (usb_submit_urb(pm->irq, GFP_KERNEL)) {
		error = -EIO;
		goto fail4;
	}

	error = input_register_device(pm->input);
	if (error)
		goto fail5;


	
	pm->requires_update = UPDATE_PULSE_ASLEEP | UPDATE_PULSE_AWAKE | UPDATE_PULSE_MODE | UPDATE_STATIC_BRIGHTNESS;
	powermate_pulse_led(pm, 0x80, 255, 0, 1, 0); 

	usb_set_intfdata(intf, pm);
	return 0;

 fail5:	usb_kill_urb(pm->irq);
 fail4:	usb_free_urb(pm->config);
 fail3:	usb_free_urb(pm->irq);
 fail2:	powermate_free_buffers(udev, pm);
 fail1:	input_free_device(input_dev);
	kfree(pm);
	return error;
}

static void powermate_disconnect(struct usb_interface *intf)
{
	struct powermate_device *pm = usb_get_intfdata (intf);

	usb_set_intfdata(intf, NULL);
	if (pm) {
		pm->requires_update = 0;
		usb_kill_urb(pm->irq);
		input_unregister_device(pm->input);
		usb_free_urb(pm->irq);
		usb_free_urb(pm->config);
		powermate_free_buffers(interface_to_usbdev(intf), pm);

		kfree(pm);
	}
}

static struct usb_device_id powermate_devices [] = {
	{ USB_DEVICE(POWERMATE_VENDOR, POWERMATE_PRODUCT_NEW) },
	{ USB_DEVICE(POWERMATE_VENDOR, POWERMATE_PRODUCT_OLD) },
	{ USB_DEVICE(CONTOUR_VENDOR, CONTOUR_JOG) },
	{ } 
};

MODULE_DEVICE_TABLE (usb, powermate_devices);

static struct usb_driver powermate_driver = {
        .name =         "powermate",
        .probe =        powermate_probe,
        .disconnect =   powermate_disconnect,
        .id_table =     powermate_devices,
};

module_usb_driver(powermate_driver);

MODULE_AUTHOR( "William R Sowerbutts" );
MODULE_DESCRIPTION( "Griffin Technology, Inc PowerMate driver" );
MODULE_LICENSE("GPL");
