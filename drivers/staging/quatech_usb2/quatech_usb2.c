
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/uaccess.h>

static bool debug;

#define DRIVER_VERSION "v2.00"
#define DRIVER_AUTHOR "Tim Gobeli, Quatech, Inc"
#define DRIVER_DESC "Quatech USB 2.0 to Serial Driver"

#define	USB_VENDOR_ID_QUATECH 0x061d	
#define QUATECH_SSU2_100 0xC120		
#define QUATECH_DSU2_100 0xC140		
#define QUATECH_DSU2_400 0xC150		
#define QUATECH_QSU2_100 0xC160		
#define QUATECH_QSU2_400 0xC170		
#define QUATECH_ESU2_100 0xC1A0		
#define QUATECH_ESU2_400 0xC180		


#define QU2BOXPWRON 0x8000		
#define QU2BOX232 0x40			
#define QU2BOXSPD9600 0x60		
#define QT2_FIFO_DEPTH 1024			
#define QT2_TX_HEADER_LENGTH	5

#define USBD_TRANSFER_DIRECTION_IN    0xc0
#define USBD_TRANSFER_DIRECTION_OUT   0x40

#define QT_SET_GET_DEVICE		0xc2
#define QT_OPEN_CLOSE_CHANNEL		0xca
#define QT2_GET_SET_REGISTER			0xc0
#define QT2_GET_SET_UART			0xc1
#define QT2_HW_FLOW_CONTROL_MASK		0xc5
#define QT2_SW_FLOW_CONTROL_MASK		0xc6
#define QT2_SW_FLOW_CONTROL_DISABLE		0xc7
#define QT2_BREAK_CONTROL			0xc8
#define QT2_STOP_RECEIVE			0xe0
#define QT2_FLUSH_DEVICE			0xc4
#define QT2_GET_SET_QMCR			0xe1

#define QT2_FLUSH_RX			0x00
#define QT2_FLUSH_TX			0x01

#define QT2_SERIAL_MCR_DTR	0x01
#define QT2_SERIAL_MCR_RTS	0x02
#define QT2_SERIAL_MCR_LOOP	0x10

#define QT2_SERIAL_MSR_CTS	0x10
#define QT2_SERIAL_MSR_CD	0x80
#define QT2_SERIAL_MSR_RI	0x40
#define QT2_SERIAL_MSR_DSR	0x20
#define QT2_SERIAL_MSR_MASK	0xf0

#define QT2_SERIAL_8_DATA	0x03
#define QT2_SERIAL_7_DATA	0x02
#define QT2_SERIAL_6_DATA	0x01
#define QT2_SERIAL_5_DATA	0x00

#define QT2_SERIAL_ODD_PARITY	0x08
#define QT2_SERIAL_EVEN_PARITY	0x18
#define QT2_SERIAL_TWO_STOPB	0x04
#define QT2_SERIAL_ONE_STOPB	0x00

#define QT2_MAX_BAUD_RATE	921600
#define QT2_MAX_BAUD_REMAINDER	4608

#define QT2_SERIAL_LSR_OE	0x02
#define QT2_SERIAL_LSR_PE	0x04
#define QT2_SERIAL_LSR_FE	0x08
#define QT2_SERIAL_LSR_BI	0x10

#define QT2_LSR_TEMT     0x40

#define  QT2_XMT_HOLD_REGISTER          0x00
#define  QT2_XVR_BUFFER_REGISTER        0x00
#define  QT2_FIFO_CONTROL_REGISTER      0x02
#define  QT2_LINE_CONTROL_REGISTER      0x03
#define  QT2_MODEM_CONTROL_REGISTER     0x04
#define  QT2_LINE_STATUS_REGISTER       0x05
#define  QT2_MODEM_STATUS_REGISTER      0x06

#define THISCHAR	((unsigned char *)(urb->transfer_buffer))[i]
#define NEXTCHAR	((unsigned char *)(urb->transfer_buffer))[i + 1]
#define THIRDCHAR	((unsigned char *)(urb->transfer_buffer))[i + 2]
#define FOURTHCHAR	((unsigned char *)(urb->transfer_buffer))[i + 3]
#define FIFTHCHAR	((unsigned char *)(urb->transfer_buffer))[i + 4]

static const struct usb_device_id quausb2_id_table[] = {
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_SSU2_100)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_DSU2_100)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_DSU2_400)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_QSU2_100)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_QSU2_400)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_ESU2_100)},
	{USB_DEVICE(USB_VENDOR_ID_QUATECH, QUATECH_ESU2_400)},
	{}	
};

MODULE_DEVICE_TABLE(usb, quausb2_id_table);

static struct usb_driver quausb2_usb_driver = {
	.name = "quatech-usb2-serial",
	.probe = usb_serial_probe,
	.disconnect = usb_serial_disconnect,
	.id_table = quausb2_id_table,
};

/**
 * quatech2_port: Structure in which to keep all the messy stuff that this
 * driver needs alongside the usb_serial_port structure
 * @read_urb_busy: Flag indicating that port->read_urb is in use
 * @close_pending: flag indicating that this port is in the process of
 * being closed (and so no new reads / writes should be started).
 * @shadowLSR: Last received state of the line status register, holds the
 * value of the line status flags from the port
 * @shadowMSR: Last received state of the modem status register, holds
 * the value of the modem status received from the port
 * @rcv_flush: Flag indicating that a receive flush has occurred on
 * the hardware.
 * @xmit_flush: Flag indicating that a transmit flush has been processed by
 * the hardware.
 * @tx_pending_bytes: Number of bytes waiting to be sent. This total
 * includes the size (excluding header) of URBs that have been submitted but
 * have not yet been sent to to the device, and bytes that have been sent out
 * of the port but not yet reported sent by the "xmit_empty" messages (which
 * indicate the number of bytes sent each time they are received, despite the
 * misleading name).
 * - Starts at zero when port is initialised.
 * - is incremented by the size of the data to be written (no headers)
 * each time a write urb is dispatched.
 * - is decremented each time a "transmit empty" message is received
 * by the driver in the data stream.
 * @lock: Mutex to lock access to this structure when we need to ensure that
 * races don't occur to access bits of it.
 * @open_count: The number of uses of the port currently having
 * it open, i.e. the reference count.
 */
struct quatech2_port {
	int	magic;
	bool	read_urb_busy;
	bool	close_pending;
	__u8	shadowLSR;
	__u8	shadowMSR;
	bool	rcv_flush;
	bool	xmit_flush;
	int	tx_pending_bytes;
	struct mutex modelock;
	int	open_count;

	char	active;		
	unsigned char		*xfer_to_tty_buffer;
	wait_queue_head_t	wait;
	__u8	shadowLCR;	
	__u8	shadowMCR;	
	char	RxHolding;
	struct semaphore	pend_xmit_sem;	
	spinlock_t lock;
};

struct quatech2_dev {
	bool	ReadBulkStopped;
	char	open_ports;
	struct usb_serial_port *current_port;
	int	buffer_size;
};

struct qt2_status_data {
	__u8 line_status;
	__u8 modem_status;
};

static int qt2_boxpoweron(struct usb_serial *serial);
static int qt2_boxsetQMCR(struct usb_serial *serial, __u16 Uart_Number,
			__u8 QMCR_Value);
static int port_paranoia_check(struct usb_serial_port *port,
			const char *function);
static int serial_paranoia_check(struct usb_serial *serial,
			 const char *function);
static inline struct quatech2_port *qt2_get_port_private(struct usb_serial_port
			*port);
static inline void qt2_set_port_private(struct usb_serial_port *port,
			struct quatech2_port *data);
static inline struct quatech2_dev *qt2_get_dev_private(struct usb_serial
			*serial);
static inline void qt2_set_dev_private(struct usb_serial *serial,
			struct quatech2_dev *data);
static int qt2_openboxchannel(struct usb_serial *serial, __u16
			Uart_Number, struct qt2_status_data *pDeviceData);
static int qt2_closeboxchannel(struct usb_serial *serial, __u16
			Uart_Number);
static int qt2_conf_uart(struct usb_serial *serial,  unsigned short Uart_Number,
			 unsigned short divisor, unsigned char LCR);
static void qt2_read_bulk_callback(struct urb *urb);
static void qt2_write_bulk_callback(struct urb *urb);
static void qt2_process_line_status(struct usb_serial_port *port,
			      unsigned char LineStatus);
static void qt2_process_modem_status(struct usb_serial_port *port,
			       unsigned char ModemStatus);
static void qt2_process_xmit_empty(struct usb_serial_port *port,
	unsigned char fourth_char, unsigned char fifth_char);
static void qt2_process_port_change(struct usb_serial_port *port,
			      unsigned char New_Current_Port);
static void qt2_process_rcv_flush(struct usb_serial_port *port);
static void qt2_process_xmit_flush(struct usb_serial_port *port);
static void qt2_process_rx_char(struct usb_serial_port *port,
				unsigned char data);
static int qt2_box_get_register(struct usb_serial *serial,
		unsigned char uart_number, unsigned short register_num,
		__u8 *pValue);
static int qt2_box_set_register(struct usb_serial *serial,
		unsigned short Uart_Number, unsigned short Register_Num,
		unsigned short Value);
static int qt2_boxsetuart(struct usb_serial *serial, unsigned short Uart_Number,
		unsigned short default_divisor, unsigned char default_LCR);
static int qt2_boxsethw_flowctl(struct usb_serial *serial,
		unsigned int UartNumber, bool bSet);
static int qt2_boxsetsw_flowctl(struct usb_serial *serial, __u16 UartNumber,
		unsigned char stop_char,  unsigned char start_char);
static int qt2_boxunsetsw_flowctl(struct usb_serial *serial, __u16 UartNumber);
static int qt2_boxstoprx(struct usb_serial *serial, unsigned short uart_number,
			 unsigned short stop);

static int qt2_calc_num_ports(struct usb_serial *serial)
{
	int num_ports;
	int flag_as_400;
	switch (serial->dev->descriptor.idProduct) {
	case QUATECH_SSU2_100:
		num_ports = 1;
		break;

	case QUATECH_DSU2_400:
		flag_as_400 = true;
	case QUATECH_DSU2_100:
		num_ports = 2;
	break;

	case QUATECH_QSU2_400:
		flag_as_400 = true;
	case QUATECH_QSU2_100:
		num_ports = 4;
	break;

	case QUATECH_ESU2_400:
		flag_as_400 = true;
	case QUATECH_ESU2_100:
		num_ports = 8;
	break;
	default:
	num_ports = 1;
	break;
	}
	return num_ports;
}

static int qt2_attach(struct usb_serial *serial)
{
	struct usb_serial_port *port;
	struct quatech2_port *qt2_port;	
	struct quatech2_dev  *qt2_dev;	
	int i;
	
	struct usb_endpoint_descriptor *endpoint;
	struct usb_host_interface *iface_desc;
	struct usb_serial_port *port0;	

	dbg("%s(): Endpoints: %d bulk in, %d bulk out, %d interrupt in",
			__func__, serial->num_bulk_in,
			serial->num_bulk_out, serial->num_interrupt_in);
	if ((serial->num_bulk_in != 1) || (serial->num_bulk_out != 1)) {
		dbg("Device has wrong number of bulk endpoints!");
		return -ENODEV;
	}
	iface_desc = serial->interface->cur_altsetting;

	qt2_dev = kzalloc(sizeof(*qt2_dev), GFP_KERNEL);
	if (!qt2_dev) {
		dbg("%s: kmalloc for quatech2_dev failed!",
		    __func__);
		return -ENOMEM;
	}
	qt2_dev->open_ports = 0;	
	qt2_set_dev_private(serial, qt2_dev);	

	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		qt2_port = kzalloc(sizeof(*qt2_port), GFP_KERNEL);
		if (!qt2_port) {
			dbg("%s: kmalloc for quatech2_port (%d) failed!.",
			    __func__, i);
			return -ENOMEM;
		}
		
		qt2_port->open_count = 0;	
		spin_lock_init(&qt2_port->lock);
		mutex_init(&qt2_port->modelock);
		qt2_set_port_private(port, qt2_port);
	}

	if (serial_paranoia_check(serial, __func__))
		return -ENODEV;
	port0 = serial->port[0]; 

	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;
		if ((endpoint->bEndpointAddress & 0x80) &&
			((endpoint->bmAttributes & 3) == 0x02)) {
			
			dbg("found bulk in at %#.2x",
				endpoint->bEndpointAddress);
		}

		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
			((endpoint->bmAttributes & 3) == 0x02)) {
			
			dbg("found bulk out at %#.2x",
				endpoint->bEndpointAddress);
			qt2_dev->buffer_size = endpoint->wMaxPacketSize;
			
		}
	}	

	
	if (qt2_boxpoweron(serial) < 0) {
		dbg("qt2_boxpoweron() failed");
		goto startup_error;
	}
	
	for (i = 0; i < serial->num_ports; ++i) {
		if (qt2_boxsetQMCR(serial, i, QU2BOX232) < 0) {
			dbg("qt2_boxsetQMCR() on port %d failed",
				i);
			goto startup_error;
		}
	}

	return 0;

startup_error:
	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		qt2_port = qt2_get_port_private(port);
		kfree(qt2_port);
		qt2_set_port_private(port, NULL);
	}
	qt2_dev = qt2_get_dev_private(serial);
	kfree(qt2_dev);
	qt2_set_dev_private(serial, NULL);

	dbg("Exit fail %s\n", __func__);
	return -EIO;
}

static void qt2_release(struct usb_serial *serial)
{
	struct usb_serial_port *port;
	struct quatech2_port *qt_port;
	int i;

	dbg("enterting %s", __func__);

	for (i = 0; i < serial->num_ports; i++) {
		port = serial->port[i];
		if (!port)
			continue;

		qt_port = usb_get_serial_port_data(port);
		kfree(qt_port);
		usb_set_serial_port_data(port, NULL);
	}
}
int qt2_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct usb_serial *serial;	
	struct usb_serial_port *port0;	
	struct quatech2_port *port_extra;       
	struct quatech2_port *port0_extra;	
	struct quatech2_dev *dev_extra;		
	struct qt2_status_data ChannelData;
	unsigned short default_divisor = QU2BOXSPD9600;
	unsigned char  default_LCR = QT2_SERIAL_8_DATA;
	int status;
	int result;

	if (port_paranoia_check(port, __func__))
		return -ENODEV;

	dbg("%s(): port %d", __func__, port->number);

	serial = port->serial;	
	if (serial_paranoia_check(serial, __func__)) {
		dbg("usb_serial struct failed sanity check");
		return -ENODEV;
	}
	dev_extra = qt2_get_dev_private(serial);
	
	if (dev_extra == NULL) {
		dbg("device extra data pointer is null");
		return -ENODEV;
	}
	port0 = serial->port[0]; 
	if (port_paranoia_check(port0, __func__)) {
		dbg("port0 usb_serial_port struct failed sanity check");
		return -ENODEV;
	}

	port_extra = qt2_get_port_private(port);
	port0_extra = qt2_get_port_private(port0);
	if (port_extra == NULL || port0_extra == NULL) {
		dbg("failed to get private data for port or port0");
		return -ENODEV;
	}

	
	
	status = qt2_openboxchannel(serial, port->number,
			&ChannelData);
	if (status < 0) {
		dbg("qt2_openboxchannel on channel %d failed",
		    port->number);
		return status;
	}
	port_extra->shadowLSR = ChannelData.line_status &
			(QT2_SERIAL_LSR_OE | QT2_SERIAL_LSR_PE |
			QT2_SERIAL_LSR_FE | QT2_SERIAL_LSR_BI);
	port_extra->shadowMSR = ChannelData.modem_status &
			(QT2_SERIAL_MSR_CTS | QT2_SERIAL_MSR_DSR |
			QT2_SERIAL_MSR_RI | QT2_SERIAL_MSR_CD);

	dbg("qt2_openboxchannel on channel %d completed.",
	    port->number);

	
	status = qt2_conf_uart(serial, port->number, default_divisor,
				default_LCR);
	if (status < 0) {
		dbg("qt2_conf_uart() failed on channel %d",
		    port->number);
		return status;
	}
	dbg("qt2_conf_uart() completed on channel %d",
		port->number);

	dbg("port0 bulk in endpoint is %#.2x", port0->bulk_in_endpointAddress);
	dbg("port0 bulk out endpoint is %#.2x",
		port0->bulk_out_endpointAddress);

	if (port->write_urb == NULL) {
		dbg("port->write_urb == NULL, allocating one");
		port->write_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!port->write_urb) {
			err("Allocating write URB failed");
			return -ENOMEM;
		}
		
		port->bulk_out_size = dev_extra->buffer_size;
		port->bulk_out_buffer = kmalloc(port->bulk_out_size,
						GFP_KERNEL);
		if (!port->bulk_out_buffer) {
			err("Couldn't allocate bulk_out_buffer");
			return -ENOMEM;
		}
	}
	if (serial->dev == NULL)
		dbg("serial->dev == NULL");
	dbg("port->bulk_out_size is %d", port->bulk_out_size);

	usb_fill_bulk_urb(port->write_urb, serial->dev,
			usb_sndbulkpipe(serial->dev,
			port0->bulk_out_endpointAddress),
			port->bulk_out_buffer,
			port->bulk_out_size,
			qt2_write_bulk_callback,
			port);
	port_extra->tx_pending_bytes = 0;

	if (dev_extra->open_ports == 0) {
		usb_fill_bulk_urb(port0->read_urb, serial->dev,
			usb_rcvbulkpipe(serial->dev,
			port0->bulk_in_endpointAddress),
			port0->bulk_in_buffer,
			port0->bulk_in_size,
			qt2_read_bulk_callback, serial);
		dbg("port0 bulk in URB initialised");

		
		dev_extra->ReadBulkStopped = false;
		port_extra->read_urb_busy = true;
		result = usb_submit_urb(port->read_urb, GFP_KERNEL);
		if (result) {
			dev_err(&port->dev,
				 "%s(): Error %d submitting bulk in urb",
				__func__, result);
			port_extra->read_urb_busy = false;
			dev_extra->ReadBulkStopped = true;
		}

		dev_extra->current_port = port;
	}

	
	init_waitqueue_head(&port_extra->wait);
	
	port_extra->open_count++;

	qt2_set_port_private(port, port_extra);
	qt2_set_port_private(serial->port[0], port0_extra);
	qt2_set_dev_private(serial, dev_extra);

	dev_extra->open_ports++; 

	return 0;
}

/* Setting close_pending should keep new data from being written out,
 * once all the data in the enpoint buffers is moved out we won't get
 * any more. */
static void qt2_close(struct usb_serial_port *port)
{
	
	unsigned long jift;
	struct quatech2_port *port_extra;	
	struct usb_serial *serial;	
	struct quatech2_dev *dev_extra; 
	__u8  lsr_value = 0;	
	int status;	

	dbg("%s(): port %d", __func__, port->number);
	serial = port->serial;	
	dev_extra = qt2_get_dev_private(serial);
	
	port_extra = qt2_get_port_private(port); 

	
	port_extra->close_pending = true;
	dbg("%s(): port_extra->close_pending = true", __func__);
	jift = jiffies + (10 * HZ);	
	do {
		status = qt2_box_get_register(serial, port->number,
			QT2_LINE_STATUS_REGISTER, &lsr_value);
		if (status < 0) {
			dbg("%s(): qt2_box_get_register failed", __func__);
			break;
		}
		if ((lsr_value & QT2_LSR_TEMT)) {
			dbg("UART done sending");
			break;
		}
		schedule();
	} while (jiffies <= jift);

	status = qt2_closeboxchannel(serial, port->number);
	if (status < 0)
		dbg("%s(): port %d qt2_box_open_close_channel failed",
			__func__, port->number);
	usb_free_urb(port->write_urb);
	kfree(port->bulk_out_buffer);
	port->bulk_out_buffer = NULL;
	port->bulk_out_size = 0;

	
	port_extra->open_count--;
	
	dev_extra->open_ports--;
	dbg("%s(): Exit, dev_extra->open_ports  = %d", __func__,
		dev_extra->open_ports);
}

/**
 * qt2_write - write bytes from the tty layer out to the USB device.
 * @buf: The data to be written, size at least count.
 * @count: The number of bytes requested for transmission.
 * @return The number of bytes actually accepted for transmission to the device.
 */
static int qt2_write(struct tty_struct *tty, struct usb_serial_port *port,
		const unsigned char *buf, int count)
{
	struct usb_serial *serial;	
	__u8 header_array[5];	
	struct quatech2_port *port_extra;	
	int result;

	serial = port->serial; 
	port_extra = qt2_get_port_private(port); 
	if (serial == NULL)
		return -ENODEV;
	dbg("%s(): port %d, requested to write %d bytes, %d already pending",
		__func__, port->number, count, port_extra->tx_pending_bytes);

	if (count <= 0)	{
		dbg("%s(): write request of <= 0 bytes", __func__);
		return 0;	/* no bytes written */
	}

	if ((port->write_urb->status == -EINPROGRESS)) {
		
		dbg("%s(): already writing, port->write_urb->status == "
			"-EINPROGRESS", __func__);
		
		return 0;
	} else if (port_extra->tx_pending_bytes >= QT2_FIFO_DEPTH) {
		dbg("%s(): port transmit buffer is full!", __func__);
		
		return 0;
	}

	if (count > port->bulk_out_size - QT2_TX_HEADER_LENGTH) {
		count = port->bulk_out_size - QT2_TX_HEADER_LENGTH;
		dbg("%s(): write request bigger than urb, only accepting "
			"%d bytes", __func__, count);
	}
	if (count > (QT2_FIFO_DEPTH - port_extra->tx_pending_bytes)) {
		count = QT2_FIFO_DEPTH - port_extra->tx_pending_bytes;
		dbg("%s(): not enough room in buffer, only accepting %d bytes",
			__func__, count);
	}
	
	header_array[0] = 0x1b;
	header_array[1] = 0x1b;
	header_array[2] = (__u8)port->number;
	header_array[3] = (__u8)count;
	header_array[4] = (__u8)count >> 8;
	
	memcpy(port->write_urb->transfer_buffer, header_array,
		QT2_TX_HEADER_LENGTH);
	
	memcpy(port->write_urb->transfer_buffer + 5, buf, count);

	dbg("%s(): first data byte to send = %#.2x", __func__, *buf);

	
	usb_fill_bulk_urb(port->write_urb, serial->dev,
			usb_sndbulkpipe(serial->dev,
			port->bulk_out_endpointAddress),
			port->write_urb->transfer_buffer, count + 5,
			(qt2_write_bulk_callback), port);
	
	result = usb_submit_urb(port->write_urb, GFP_ATOMIC);
	if (result) {
		
		result = 0;	/* return 0 as nothing got written */
		dbg("%s(): failed submitting write urb, error %d",
			__func__, result);
	} else {
		port_extra->tx_pending_bytes += count;
		result = count;	/* return number of bytes written, i.e. count */
		dbg("%s(): submitted write urb, wrote %d bytes, "
			"total pending bytes %d",
			__func__, result, port_extra->tx_pending_bytes);
	}
	return result;
}

/* This is used by the next layer up to know how much space is available
 * in the buffer on the device. It is used on a device closure to avoid
 * calling close() until the buffer is reported to be empty.
 * The returned value must never go down by more than the number of bytes
 * written for correct behaviour further up the driver stack, i.e. if I call
 * it, then write 6 bytes, then call again I should get 6 less, or possibly
 * only 5 less if one was written in the meantime, etc. I should never get 7
 * less (or any bigger number) because I only wrote 6 bytes.
 */
static int qt2_write_room(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
		
	struct quatech2_port *port_extra;	
	int room = 0;
	port_extra = qt2_get_port_private(port);

	if (port_extra->close_pending == true) {
		dbg("%s(): port_extra->close_pending == true", __func__);
		return -ENODEV;
	}

	room = (QT2_FIFO_DEPTH - port_extra->tx_pending_bytes);
	
	if (room > port->bulk_out_size - QT2_TX_HEADER_LENGTH)
		room = port->bulk_out_size - QT2_TX_HEADER_LENGTH;
	

	dbg("%s(): port %d: write room is %d", __func__, port->number, room);
	return room;
}

static int qt2_chars_in_buffer(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	
	struct quatech2_port *port_extra;	
	port_extra = qt2_get_port_private(port);

	dbg("%s(): port %d: chars_in_buffer = %d", __func__,
		port->number, port_extra->tx_pending_bytes);
	return port_extra->tx_pending_bytes;
}

static int qt2_ioctl(struct tty_struct *tty,
		     unsigned int cmd, unsigned long arg)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	__u8 mcr_value;	
	__u8 msr_value; 
	unsigned short prev_msr_value; 
	struct quatech2_port *port_extra;	
	DECLARE_WAITQUEUE(wait, current);
	

	unsigned int value;
	unsigned int UartNumber;

	if (serial == NULL)
		return -ENODEV;
	UartNumber = tty->index - serial->minor;
	port_extra = qt2_get_port_private(port);

	dbg("%s(): port %d, UartNumber %d, tty =0x%p", __func__,
	    port->number, UartNumber, tty);

	if (cmd == TIOCMBIS || cmd == TIOCMBIC) {
		if (qt2_box_get_register(port->serial, UartNumber,
			QT2_MODEM_CONTROL_REGISTER, &mcr_value) < 0)
			return -ESPIPE;
		if (copy_from_user(&value, (unsigned int *)arg,
			sizeof(value)))
			return -EFAULT;

		switch (cmd) {
		case TIOCMBIS:
			if (value & TIOCM_RTS)
				mcr_value |= QT2_SERIAL_MCR_RTS;
			if (value & TIOCM_DTR)
				mcr_value |= QT2_SERIAL_MCR_DTR;
			if (value & TIOCM_LOOP)
				mcr_value |= QT2_SERIAL_MCR_LOOP;
		break;
		case TIOCMBIC:
			if (value & TIOCM_RTS)
				mcr_value &= ~QT2_SERIAL_MCR_RTS;
			if (value & TIOCM_DTR)
				mcr_value &= ~QT2_SERIAL_MCR_DTR;
			if (value & TIOCM_LOOP)
				mcr_value &= ~QT2_SERIAL_MCR_LOOP;
		break;
		default:
		break;
		}	
		if (qt2_box_set_register(port->serial,  UartNumber,
		    QT2_MODEM_CONTROL_REGISTER, mcr_value) < 0) {
			return -ESPIPE;
		} else {
			port_extra->shadowMCR = mcr_value;
			return 0;
		}
	} else if (cmd == TIOCMIWAIT) {
		dbg("%s() port %d, cmd == TIOCMIWAIT enter",
			__func__, port->number);
		prev_msr_value = port_extra->shadowMSR  & QT2_SERIAL_MSR_MASK;
		barrier();
		__set_current_state(TASK_INTERRUPTIBLE);
		while (1) {
			add_wait_queue(&port_extra->wait, &wait);
			schedule();
			dbg("%s(): port %d, cmd == TIOCMIWAIT here\n",
				__func__, port->number);
			remove_wait_queue(&port_extra->wait, &wait);
			
			if (signal_pending(current))
				return -ERESTARTSYS;
			set_current_state(TASK_INTERRUPTIBLE);
			msr_value = port_extra->shadowMSR & QT2_SERIAL_MSR_MASK;
			if (msr_value == prev_msr_value) {
				__set_current_state(TASK_RUNNING);
				return -EIO;  
			}
			if ((arg & TIOCM_RNG &&
				((prev_msr_value & QT2_SERIAL_MSR_RI) ==
					(msr_value & QT2_SERIAL_MSR_RI))) ||
				(arg & TIOCM_DSR &&
				((prev_msr_value & QT2_SERIAL_MSR_DSR) ==
					(msr_value & QT2_SERIAL_MSR_DSR))) ||
				(arg & TIOCM_CD &&
				((prev_msr_value & QT2_SERIAL_MSR_CD) ==
					(msr_value & QT2_SERIAL_MSR_CD))) ||
				(arg & TIOCM_CTS &&
				((prev_msr_value & QT2_SERIAL_MSR_CTS) ==
					(msr_value & QT2_SERIAL_MSR_CTS)))) {
				__set_current_state(TASK_RUNNING);
				return 0;
			}
		} 
	} else {
		
		dbg("%s(): No ioctl for that one. port = %d", __func__,
			port->number);
		return -ENOIOCTLCMD;
	}
}

static void qt2_set_termios(struct tty_struct *tty,
	struct usb_serial_port *port, struct ktermios *old_termios)
{
	struct usb_serial *serial; 
	int baud, divisor, remainder;
	unsigned char LCR_change_to = 0;
	int status;
	__u16 UartNumber;

	dbg("%s(): port %d", __func__, port->number);

	serial = port->serial;

	UartNumber = port->number;

	if (old_termios && !tty_termios_hw_change(old_termios, tty->termios))
		return;

	switch (tty->termios->c_cflag) {
	case CS5:
		LCR_change_to |= QT2_SERIAL_5_DATA;
		break;
	case CS6:
		LCR_change_to |= QT2_SERIAL_6_DATA;
		break;
	case CS7:
		LCR_change_to |= QT2_SERIAL_7_DATA;
		break;
	default:
	case CS8:
		LCR_change_to |= QT2_SERIAL_8_DATA;
		break;
	}

	
	if (tty->termios->c_cflag & PARENB) {
		if (tty->termios->c_cflag & PARODD)
			LCR_change_to |= QT2_SERIAL_ODD_PARITY;
		else
			LCR_change_to |= QT2_SERIAL_EVEN_PARITY;
	}
	tty->termios->c_cflag &= ~CMSPAR;

	if (tty->termios->c_cflag & CSTOPB)
		LCR_change_to |= QT2_SERIAL_TWO_STOPB;
	else
		LCR_change_to |= QT2_SERIAL_ONE_STOPB;

	baud = tty_get_baud_rate(tty);
	if (!baud) {
		
		baud = 9600;
	}
	dbg("%s(): got baud = %d", __func__, baud);

	divisor = QT2_MAX_BAUD_RATE / baud;
	remainder = QT2_MAX_BAUD_RATE % baud;
	
	if (((remainder * 2) >= baud) && (baud != 110))
		divisor++;
	dbg("%s(): setting divisor = %d, QT2_MAX_BAUD_RATE = %d , LCR = %#.2x",
	      __func__, divisor, QT2_MAX_BAUD_RATE, LCR_change_to);

	status = qt2_boxsetuart(serial, UartNumber, (unsigned short) divisor,
			    LCR_change_to);
	if (status < 0)	{
		dbg("qt2_boxsetuart() failed");
		return;
	} else {
		baud = QT2_MAX_BAUD_RATE / divisor;
		tty_encode_baud_rate(tty, baud, baud);
	}

	
	if (tty->termios->c_cflag & CRTSCTS) {
		dbg("%s(): Enabling HW flow control port %d", __func__,
		      port->number);
		
		status = qt2_boxsethw_flowctl(serial, UartNumber, true);
		if (status < 0) {
			dbg("qt2_boxsethw_flowctl() failed");
			return;
		}
	} else {
		
		dbg("%s(): disabling HW flow control port %d", __func__,
			port->number);
		status = qt2_boxsethw_flowctl(serial, UartNumber, false);
		if (status < 0)	{
			dbg("qt2_boxsethw_flowctl failed");
			return;
		}
	}
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);
		status = qt2_boxsetsw_flowctl(serial, UartNumber, stop_char,
				start_char);
		if (status < 0)
			dbg("qt2_boxsetsw_flowctl (enabled) failed");
	} else {
		
		status = qt2_boxunsetsw_flowctl(serial, UartNumber);
		if (status < 0)
			dbg("qt2_boxunsetsw_flowctl (disabling) failed");
	}
}

static int qt2_tiocmget(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;

	__u8 mcr_value;	
	__u8 msr_value;	
	unsigned int result = 0;
	int status;
	unsigned int UartNumber;

	if (serial == NULL)
		return -ENODEV;

	dbg("%s(): port %d, tty =0x%p", __func__, port->number, tty);
	UartNumber = tty->index - serial->minor;
	dbg("UartNumber is %d", UartNumber);

	status = qt2_box_get_register(port->serial, UartNumber,
			QT2_MODEM_CONTROL_REGISTER,	&mcr_value);
	if (status >= 0) {
		status = qt2_box_get_register(port->serial,  UartNumber,
				QT2_MODEM_STATUS_REGISTER, &msr_value);
	}
	if (status >= 0) {
		result = ((mcr_value & QT2_SERIAL_MCR_DTR) ? TIOCM_DTR : 0)
				
			| ((mcr_value & QT2_SERIAL_MCR_RTS)  ? TIOCM_RTS : 0)
				
			| ((msr_value & QT2_SERIAL_MSR_CTS)  ? TIOCM_CTS : 0)
				
			| ((msr_value & QT2_SERIAL_MSR_CD)  ? TIOCM_CAR : 0)
				
			| ((msr_value & QT2_SERIAL_MSR_RI)  ? TIOCM_RI : 0)
				
			| ((msr_value & QT2_SERIAL_MSR_DSR)  ? TIOCM_DSR : 0);
				
		return result;
	} else {
		return -ESPIPE;
	}
}

static int qt2_tiocmset(struct tty_struct *tty,
		       unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	__u8 mcr_value;	
	int status;
	unsigned int UartNumber;

	if (serial == NULL)
		return -ENODEV;

	UartNumber = tty->index - serial->minor;
	dbg("%s(): port %d, UartNumber %d", __func__, port->number, UartNumber);

	status = qt2_box_get_register(port->serial, UartNumber,
			QT2_MODEM_CONTROL_REGISTER, &mcr_value);
	if (status < 0)
		return -ESPIPE;

	mcr_value &= ~(QT2_SERIAL_MCR_RTS | QT2_SERIAL_MCR_DTR |
			QT2_SERIAL_MCR_LOOP);
	if (set & TIOCM_RTS)
		mcr_value |= QT2_SERIAL_MCR_RTS;
	if (set & TIOCM_DTR)
		mcr_value |= QT2_SERIAL_MCR_DTR;
	if (set & TIOCM_LOOP)
		mcr_value |= QT2_SERIAL_MCR_LOOP;

	status = qt2_box_set_register(port->serial, UartNumber,
			QT2_MODEM_CONTROL_REGISTER, mcr_value);
	if (status < 0)
		return -ESPIPE;
	else
		return 0;
}

static void qt2_break(struct tty_struct *tty, int break_state)
{
	struct usb_serial_port *port = tty->driver_data; 
	struct usb_serial *serial = port->serial;	
	struct quatech2_port *port_extra;	
	__u16 break_value;
	unsigned int result;

	port_extra = qt2_get_port_private(port);
	if (!serial) {
		dbg("%s(): port %d: no serial object", __func__, port->number);
		return;
	}

	if (break_state == -1)
		break_value = 1;
	else
		break_value = 0;
	dbg("%s(): port %d, break_value %d", __func__, port->number,
		break_value);

	mutex_lock(&port_extra->modelock);
	if (!port_extra->open_count) {
		dbg("%s(): port not open", __func__);
		goto exit;
	}

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				QT2_BREAK_CONTROL, 0x40, break_value,
				port->number, NULL, 0, 300);
exit:
	mutex_unlock(&port_extra->modelock);
	dbg("%s(): exit port %d", __func__, port->number);

}
static void qt2_throttle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	struct quatech2_port *port_extra;	
	dbg("%s(): port %d", __func__, port->number);

	port_extra = qt2_get_port_private(port);
	if (!serial) {
		dbg("%s(): enter port %d no serial object", __func__,
		      port->number);
		return;
	}

	mutex_lock(&port_extra->modelock);	
	if (!port_extra->open_count) {
		dbg("%s(): port not open", __func__);
		goto exit;
	}
	if (serial->dev->descriptor.idProduct != QUATECH_SSU2_100)
		qt2_boxstoprx(serial, port->number, 1);

	port->throttled = 1;
exit:
	mutex_unlock(&port_extra->modelock);
	dbg("%s(): port %d: setting port->throttled", __func__, port->number);
	return;
}

static void qt2_unthrottle(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct usb_serial *serial = port->serial;
	struct quatech2_port *port_extra;	
	struct usb_serial_port *port0;	
	struct quatech2_dev *dev_extra;		

	if (!serial) {
		dbg("%s() enter port %d no serial object!", __func__,
			port->number);
		return;
	}
	dbg("%s(): enter port %d", __func__, port->number);
	dev_extra = qt2_get_dev_private(serial);
	port_extra = qt2_get_port_private(port);
	port0 = serial->port[0]; 

	mutex_lock(&port_extra->modelock);
	if (!port_extra->open_count) {
		dbg("%s(): port %d not open", __func__, port->number);
		goto exit;
	}

	if (port->throttled != 0) {
		dbg("%s(): port %d: unsetting port->throttled", __func__,
		    port->number);
		port->throttled = 0;
		
		if (serial->dev->descriptor.idProduct != QUATECH_SSU2_100) {
			qt2_boxstoprx(serial,  port->number, 0);
		} else if (dev_extra->ReadBulkStopped == true) {
			usb_fill_bulk_urb(port0->read_urb, serial->dev,
				usb_rcvbulkpipe(serial->dev,
				port0->bulk_in_endpointAddress),
				port0->bulk_in_buffer,
				port0->bulk_in_size,
				qt2_read_bulk_callback,
				serial);
		}
	}
exit:
	mutex_unlock(&port_extra->modelock);
	dbg("%s(): exit port %d", __func__, port->number);
	return;
}


static int qt2_boxpoweron(struct usb_serial *serial)
{
	int result;
	__u8  Direcion;
	unsigned int pipe;
	Direcion = USBD_TRANSFER_DIRECTION_OUT;
	pipe = usb_rcvctrlpipe(serial->dev, 0);
	result = usb_control_msg(serial->dev, pipe, QT_SET_GET_DEVICE,
				Direcion, QU2BOXPWRON, 0x00, NULL, 0x00,
				5000);
	return result;
}

/*
 * qt2_boxsetQMCR Issue a QT2_GET_SET_QMCR vendor-spcific request on the
 * default control pipe. If successful return the number of bytes written,
 * otherwise return a negative error number of the problem.
 */
static int qt2_boxsetQMCR(struct usb_serial *serial, __u16 Uart_Number,
			  __u8 QMCR_Value)
{
	int result;
	__u16 PortSettings;

	PortSettings = (__u16)(QMCR_Value);

	dbg("%s(): Port = %d, PortSettings = 0x%x", __func__,
			Uart_Number, PortSettings);

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				QT2_GET_SET_QMCR, 0x40, PortSettings,
				(__u16)Uart_Number, NULL, 0, 5000);
	return result;
}

static int port_paranoia_check(struct usb_serial_port *port,
			       const char *function)
{
	if (!port) {
		dbg("%s - port == NULL", function);
		return -1;
	}
	if (!port->serial) {
		dbg("%s - port->serial == NULL\n", function);
		return -1;
	}
	return 0;
}

static int serial_paranoia_check(struct usb_serial *serial,
				 const char *function)
{
	if (!serial) {
		dbg("%s - serial == NULL\n", function);
		return -1;
	}

	if (!serial->type) {
		dbg("%s - serial->type == NULL!", function);
		return -1;
	}

	return 0;
}

static inline struct quatech2_port *qt2_get_port_private(struct usb_serial_port
		*port)
{
	return (struct quatech2_port *)usb_get_serial_port_data(port);
}

static inline void qt2_set_port_private(struct usb_serial_port *port,
		struct quatech2_port *data)
{
	usb_set_serial_port_data(port, (void *)data);
}

static inline struct quatech2_dev *qt2_get_dev_private(struct usb_serial
		*serial)
{
	return (struct quatech2_dev *)usb_get_serial_data(serial);
}
static inline void qt2_set_dev_private(struct usb_serial *serial,
		struct quatech2_dev *data)
{
	usb_set_serial_data(serial, (void *)data);
}

static int qt2_openboxchannel(struct usb_serial *serial, __u16
		Uart_Number, struct qt2_status_data *status)
{
	int result;
	__u16 length;
	__u8  Direcion;
	unsigned int pipe;
	length = sizeof(struct qt2_status_data);
	Direcion = USBD_TRANSFER_DIRECTION_IN;
	pipe = usb_rcvctrlpipe(serial->dev, 0);
	result = usb_control_msg(serial->dev, pipe, QT_OPEN_CLOSE_CHANNEL,
			Direcion, 0x00, Uart_Number, status, length, 5000);
	return result;
}
static int qt2_closeboxchannel(struct usb_serial *serial, __u16 Uart_Number)
{
	int result;
	__u8  direcion;
	unsigned int pipe;
	direcion = USBD_TRANSFER_DIRECTION_OUT;
	pipe = usb_sndctrlpipe(serial->dev, 0);
	result = usb_control_msg(serial->dev, pipe, QT_OPEN_CLOSE_CHANNEL,
		  direcion, 0, Uart_Number, NULL, 0, 5000);
	return result;
}

static int qt2_conf_uart(struct usb_serial *serial,  unsigned short Uart_Number,
		      unsigned short divisor, unsigned char LCR)
{
	int result;
	unsigned short UartNumandLCR;

	UartNumandLCR = (LCR << 8) + Uart_Number;

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				QT2_GET_SET_UART, 0x40, divisor, UartNumandLCR,
				NULL, 0, 300);
	return result;
}

static void qt2_read_bulk_callback(struct urb *urb)
{
	
	struct usb_serial *serial = urb->context;
	
	struct quatech2_dev *dev_extra = qt2_get_dev_private(serial);
	
	struct usb_serial_port *port0 = serial->port[0];
	
	struct usb_serial_port *active = dev_extra->current_port;
	
	struct quatech2_port *port0_extra = qt2_get_port_private(port0);
	
	struct quatech2_port *active_extra = qt2_get_port_private(active);
	
	struct tty_struct *tty_st;
	unsigned int RxCount;	
	unsigned int i;	
	int result;	
	bool escapeflag;	
	dbg("%s(): callback running, active port is %d", __func__,
		active->number);

	if (urb->status) {
		
		dev_extra->ReadBulkStopped = true;
		dbg("%s(): nonzero bulk read status received: %d",
			__func__, urb->status);
		return;
	}

	
	if (port_paranoia_check(port0, __func__) != 0) {
		dbg("%s - port_paranoia_check on port0 failed, exiting\n",
__func__);
		return;
	}
	if (port_paranoia_check(active, __func__) != 0) {
		dbg("%s - port_paranoia_check on current_port "
			"failed, exiting", __func__);
		return;
	}


	if (active_extra->close_pending == true) {
		
		dbg("%s - (active->close_pending == true", __func__);
		if (dev_extra->open_ports <= 0) {
			dev_extra->ReadBulkStopped = true;
			dbg("%s - (ReadBulkStopped == true;", __func__);
			return;
		}
	}

	if ((port0_extra->RxHolding == true) &&
		    (serial->dev->descriptor.idProduct == QUATECH_SSU2_100)) {
		dev_extra->ReadBulkStopped = true;
		return;
	}

	tty_st = tty_port_tty_get(&active->port);
	if (!tty_st) {
		dbg("%s - bad tty pointer - exiting", __func__);
		return;
	}
	RxCount = urb->actual_length;	

	if (RxCount) {
		
		for (i = 0; i < RxCount ; ++i) {
			
			if ((i <= (RxCount - 3)) && (THISCHAR == 0x1b)
				&& (NEXTCHAR == 0x1b)) {
				escapeflag = false;
				switch (THIRDCHAR) {
				case 0x00:
					if (i > (RxCount - 4)) {
						dbg("Illegal escape sequences "
						"in received data");
						break;
					}
					qt2_process_line_status(active,
						FOURTHCHAR);
					i += 3;
					escapeflag = true;
					break;
				case 0x01:
					if (i > (RxCount - 4)) {
						dbg("Illegal escape sequences "
						"in received data");
						break;
					}
					qt2_process_modem_status(active,
						FOURTHCHAR);
					i += 3;
					escapeflag = true;
					break;
				case 0x02:
					if (i > (RxCount - 4)) {
						dbg("Illegal escape sequences "
						"in received data");
						break;
					}
					qt2_process_xmit_empty(active,
						FOURTHCHAR, FIFTHCHAR);
					i += 4;
					escapeflag = true;
					break;
				case 0x03:
					if (i > (RxCount - 4)) {
						dbg("Illegal escape sequences "
						"in received data");
						break;
					}
					if (active_extra->open_count > 0)
						tty_flip_buffer_push(tty_st);

					dbg("Port Change: new port = %d",
						FOURTHCHAR);
					qt2_process_port_change(active,
						FOURTHCHAR);
					i += 3;
					escapeflag = true;
					active = dev_extra->current_port;
					active_extra =
						qt2_get_port_private(active);
					tty_st = tty_port_tty_get(
						&active->port);
					break;
				case 0x04:
					if (i > (RxCount - 3)) {
						dbg("Illegal escape sequences "
							"in received data");
						break;
					}
					qt2_process_rcv_flush(active);
					i += 2;
					escapeflag = true;
					break;
				case 0x05:
					
					if (i > (RxCount - 3)) {
						dbg("Illegal escape sequences "
						"in received data");
						break;
					}
					qt2_process_xmit_flush(active);
					i += 2;
					escapeflag = true;
					break;
				case 0xff:
					dbg("No status sequence");
					qt2_process_rx_char(active, THISCHAR);
					qt2_process_rx_char(active, NEXTCHAR);
					i += 2;
					break;
				default:
					qt2_process_rx_char(active, THISCHAR);
					i += 1;
					break;
				} 
				if (escapeflag == true)
					continue;
			} 
			if (tty_st && urb->actual_length) {
				tty_buffer_request_room(tty_st, 1);
				tty_insert_flip_string(tty_st, &(
						(unsigned char *)
						(urb->transfer_buffer)
					)[i], 1);
			}
		} 
		tty_flip_buffer_push(tty_st);
	} 


	usb_fill_bulk_urb(port0->read_urb, serial->dev,
		usb_rcvbulkpipe(serial->dev, port0->bulk_in_endpointAddress),
		port0->bulk_in_buffer, port0->bulk_in_size,
		qt2_read_bulk_callback, serial);
	result = usb_submit_urb(port0->read_urb, GFP_ATOMIC);
	if (result) {
		dbg("%s(): failed resubmitting read urb, error %d",
			__func__, result);
	} else {
		dbg("%s() successfully resubmitted read urb", __func__);
		if (tty_st && RxCount) {
			tty_flip_buffer_push(tty_st);
			tty_schedule_flip(tty_st);
		}
	}

	
	dbg("%s() completed", __func__);
	return;
}

static void qt2_write_bulk_callback(struct urb *urb)
{
	struct usb_serial_port *port = (struct usb_serial_port *)urb->context;
	struct usb_serial *serial = port->serial;
	dbg("%s(): port %d", __func__, port->number);
	if (!serial) {
		dbg("%s(): bad serial pointer, exiting", __func__);
		return;
	}
	if (urb->status) {
		dbg("%s(): nonzero write bulk status received: %d",
			__func__, urb->status);
		return;
	}
	
	schedule_work(&port->work);
	dbg("%s(): port %d exit", __func__, port->number);
	return;
}

static void qt2_process_line_status(struct usb_serial_port *port,
	unsigned char LineStatus)
{
	
	struct quatech2_port *port_extra = qt2_get_port_private(port);
	port_extra->shadowLSR = LineStatus & (QT2_SERIAL_LSR_OE |
		QT2_SERIAL_LSR_PE | QT2_SERIAL_LSR_FE | QT2_SERIAL_LSR_BI);
}
static void qt2_process_modem_status(struct usb_serial_port *port,
	unsigned char ModemStatus)
{
	
	struct quatech2_port *port_extra = qt2_get_port_private(port);
	port_extra->shadowMSR = ModemStatus;
	wake_up_interruptible(&port_extra->wait);
}

static void qt2_process_xmit_empty(struct usb_serial_port *port,
	unsigned char fourth_char, unsigned char fifth_char)
{
	int byte_count;
	
	struct quatech2_port *port_extra = qt2_get_port_private(port);

	byte_count = (int)(fifth_char * 16);
	byte_count +=  (int)fourth_char;
	/* byte_count indicates how many bytes the device has written out. This
	 * message appears to occur regularly, and is used in the vendor driver
	 * to keep track of the fill state of the port transmit buffer */
	port_extra->tx_pending_bytes -= byte_count;
	dbg("port %d: %d bytes reported sent, %d still pending", port->number,
			byte_count, port_extra->tx_pending_bytes);

	
}

static void qt2_process_port_change(struct usb_serial_port *port,
	unsigned char New_Current_Port)
{
	
	struct usb_serial *serial = port->serial;
	
	struct quatech2_dev *dev_extra = qt2_get_dev_private(serial);
	dev_extra->current_port = serial->port[New_Current_Port];
	
}

static void qt2_process_rcv_flush(struct usb_serial_port *port)
{
	
	struct quatech2_port *port_extra = qt2_get_port_private(port);
	port_extra->rcv_flush = true;
}
static void qt2_process_xmit_flush(struct usb_serial_port *port)
{
	
	struct quatech2_port *port_extra = qt2_get_port_private(port);
	port_extra->xmit_flush = true;
}

static void qt2_process_rx_char(struct usb_serial_port *port,
	unsigned char data)
{
	
	struct tty_struct *tty = tty_port_tty_get(&(port->port));
	
	struct urb *urb = port->serial->port[0]->read_urb;

	if (tty && urb->actual_length) {
		tty_buffer_request_room(tty, 1);
		tty_insert_flip_string(tty, &data, 1);
		
		
	}
}

static int qt2_box_get_register(struct usb_serial *serial,
		unsigned char uart_number, unsigned short register_num,
		__u8 *pValue)
{
	int result;
	result = usb_control_msg(serial->dev, usb_rcvctrlpipe(serial->dev, 0),
			QT2_GET_SET_REGISTER, 0xC0, register_num,
			uart_number, (void *)pValue, sizeof(*pValue), 300);
	return result;
}

static int qt2_box_set_register(struct usb_serial *serial,
		unsigned short Uart_Number, unsigned short Register_Num,
		unsigned short Value)
{
	int result;
	unsigned short reg_and_byte;

	reg_and_byte = Value;
	reg_and_byte = reg_and_byte << 8;
	reg_and_byte = reg_and_byte + Register_Num;

	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			QT2_GET_SET_REGISTER, 0x40, reg_and_byte,
			Uart_Number, NULL, 0, 300);
	return result;
}

static int qt2_boxsetuart(struct usb_serial *serial, unsigned short Uart_Number,
		unsigned short default_divisor, unsigned char default_LCR)
{
	unsigned short UartNumandLCR;

	UartNumandLCR = (default_LCR << 8) + Uart_Number;

	return usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			QT2_GET_SET_UART, 0x40, default_divisor, UartNumandLCR,
			NULL, 0, 300);
}

static int qt2_boxsethw_flowctl(struct usb_serial *serial,
		unsigned int UartNumber, bool bSet)
{
	__u8 MCR_Value = 0;
	__u8 MSR_Value = 0;
	__u16 MOUT_Value = 0;

	if (bSet == true) {
		MCR_Value =  QT2_SERIAL_MCR_RTS;
	} else {
		
		MCR_Value =  0;
	}
	MOUT_Value = MCR_Value << 8;

	if (bSet == true) {
		MSR_Value = QT2_SERIAL_MSR_CTS;
	} else {
		
		MSR_Value = 0;
	}
	MOUT_Value |= MSR_Value;
	return usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			QT2_HW_FLOW_CONTROL_MASK, 0x40, MOUT_Value, UartNumber,
			NULL, 0, 300);
}

static int qt2_boxsetsw_flowctl(struct usb_serial *serial, __u16 UartNumber,
			unsigned char stop_char,  unsigned char start_char)
{
	__u16 nSWflowout;

	nSWflowout = start_char << 8;
	nSWflowout = (unsigned short)stop_char;
	return usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			QT2_SW_FLOW_CONTROL_MASK, 0x40, nSWflowout, UartNumber,
			NULL, 0, 300);
}

static int qt2_boxunsetsw_flowctl(struct usb_serial *serial, __u16 UartNumber)
{
	return usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
			QT2_SW_FLOW_CONTROL_DISABLE, 0x40, 0, UartNumber, NULL,
			0, 300);
}

static int qt2_boxstoprx(struct usb_serial *serial, unsigned short uart_number,
		unsigned short stop)
{
	return usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
		QT2_STOP_RECEIVE, 0x40, stop, uart_number, NULL, 0, 300);
}



static struct usb_serial_driver quatech2_device = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "quatech_usb2",
	},
	.description = DRIVER_DESC,
	.id_table = quausb2_id_table,
	.num_ports = 8,
	.open = qt2_open,
	.close = qt2_close,
	.write = qt2_write,
	.write_room = qt2_write_room,
	.chars_in_buffer = qt2_chars_in_buffer,
	.throttle = qt2_throttle,
	.unthrottle = qt2_unthrottle,
	.calc_num_ports = qt2_calc_num_ports,
	.ioctl = qt2_ioctl,
	.set_termios = qt2_set_termios,
	.break_ctl = qt2_break,
	.tiocmget = qt2_tiocmget,
	.tiocmset = qt2_tiocmset,
	.attach = qt2_attach,
	.release = qt2_release,
	.read_bulk_callback = qt2_read_bulk_callback,
	.write_bulk_callback = qt2_write_bulk_callback,
};

static struct usb_serial_driver * const serial_drivers[] = {
	&quatech2_device, NULL
};

module_usb_serial_driver(quausb2_usb_driver, serial_drivers);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");
