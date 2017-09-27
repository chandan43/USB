#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/usb.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
/*Driver INFO*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("beingchandanjha@gamil.com");
MODULE_DESCRIPTION("My first USB device driver");
MODULE_VERSION(".2");
/* Define these values to match your devices */
#define USB_SKEL_VENDOR_ID	0x0781
#define USB_SKEL_PRODUCT_ID	0x5567
/**
 * USB_DEVICE - macro used to describe a specific usb device
 * @vend: the 16 bit USB Vendor ID
 * @prod: the 16 bit USB Product ID
 *
 * This macro is used to create a struct usb_device_id that matches a
 * specific device.
 */
/* table of devices that work with this driver */
static const struct usb_device_id usb_table[]={
	{USB_DEVICE(USB_SKEL_VENDOR_ID,USB_SKEL_PRODUCT_ID)},
	{}  /* Terminating entry */,
};
/*@id_table: USB drivers use ID table to support hotplugging.
 *      Export this with MODULE_DEVICE_TABLE(usb,...).  This must be set
 *      or your driver's probe function will never get called.
*/
MODULE_DEVICE_TABLE(usb,usb_table);


/* Get a minor range for your devices from the usb maintainer */
#define USB_SKEL_MINOR_BASE	192
struct usb_dev {
	struct usb_device* udev;                 /* the usb device for this device */
	struct usb_interface * interface;       /* the interface for this device */
	unsigned char * bulk_in_buffer;         /*the buffer to receive data */
	size_t bulk_in_size;                   /*the size of the receive buffer */
	__u8	bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
	__u8	bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
	struct kref kref;              
};
/*krefs allow you to add reference counters to your objects.  If you
 * have objects that are used in multiple places and passed around, and
 * you don't have refcounts, your code is almost certainly broken.  If
 * you want refcounts, krefs are the way to go.*/

/*Macro sets up a pointer that points to the struct device_driver passed to the code . The macro to get a pointer to struct usb_dev by using:*/
#define to_usb_dev(d) container_of(d, struct usb_dev, kref);
struct struct usb_driver usb_drv;
static void usb_delete(struct kref *ref){
	struct usb_dev *dev=to_usb_dev(kref);
	usb_put_dev(dev->udev); /*release a use of the usb device structure.Must be called when a user of a device is finished with it*/
	kfree(dev->bulk_in_buffer);  /*Free buffer*/
	kfree (dev);   /*Free device*/
}

static int usb_open(struct inode *inodep, struct file *filep){
	struct usb_dev *dev;
	struct usb_interface *interface;
	int subminor;
	int retval=0;
	/* Be sure to use iminor to obtain the minor number from the inode structure, and make sure that it corresponds to a device that your driver is actually prepared to handle.*/
	subminor=iminor(inodep);   
	interface=usb_find_interface(&usb_drv,subminor); /* find usb_interface pointer for driver and device */
	if(!interface){
		pr_err("%s: Can't find device for minor %d",__func__, subminor);
		retval=-ENODEV;
		goto exit;
	}
	dev = usb_get_intfdata(interface);	/* To retrieve the data, the function usb_get_intfdata should be called:*/
	if(!dev){
		retval=-ENODEV;
		goto exit;
	}
	/* increment our usage count for the device */
	kref_get(&dev->kref);
	/* save our object in the file's private structure */
	filep->private_data=dev;
	return 0;
exit:
		return retval;
}
static  int usb_release(struct inode *inodep, struct file *filep){
	struct usb_dev *dev;
	dev=(struct usb_dev *)filep->private_data;
	if (dev == NULL)
		return -ENODEV;
	/* decrement the count on our device */
	kref_put(&dev->kref, usb_delete);
	return 0;
}

static int  ssize_t usb_read(struct file *filep, char __user *buffer, size_t count, loff_t *offset){
	struct usb_dev *dev;
	dev=(struct usb_dev *)filep->private_data;
	int retval = 0;
	if(dev == NULL)
		return -ENODEV;
	/* do a blocking bulk read to get data from the device */
	/*usb_bulk_msg : This function sends a simple bulk message to a specified endpoint and waits for the message to complete, or timeout. */
	retval=usb_bulk_msg(dev->udev,usb_rcvbulkpipe(dev->udev,dev->bulk_in_endpointAddr),dev->bulk_in_buffer,min(dev->bulk_in_size,count), &count,HZ*10);
	/*P1: A pointer to the USB device
	 *p2: The specific endpoint of the USB device to which this bulk message is to be sent. This value is created with a call to either usb_sndbulkpipe or usb_rcvbulkpipe.
	 *p3:A pointer to the data to send to the device if this is an OUT endpoint. If this is an IN endpoint, this is a pointer to where the data should be placed after being read from the device.
	 *p4:The length of the buffer that is pointed to by the data parameter.min is defined in kernel.h
	 *p5:A pointer to where the function places the actual number of bytes that have either been transferred to the device or received from the device, depending on the direction of the endpoint.
	*p6: The amount of time, in jiffies, that should be waited before timing out.
	*HZ : value for i386 was changed to 1000, yeilding a jiffy interval of 1 ms. Recently (2.6.13) the kernel changed HZ for i386 to 250. (1000 was deemed too high).*/
 	/*If successful, it returns 0, otherwise a negative error number. */
	/* if the read was successful, copy the data to userspace */
	if(!retval){
		if(copy_to_user(buffer, dev->bulk_in_buffer, count))  //  On success, this will be zero. 
			return -EFAULT;
		else 
			return count;
	}
	return retval;
}
/* (in) completion routine */
/*
 *   - Out of memory (-ENOMEM)
 *   - Unplugged device (-ENODEV)
 *   - Stalled endpoint (-EPIPE)
 *   - Too many queued ISO transfers (-EAGAIN)
 *   - Too many requested ISO frames (-EFBIG)
 *   - Invalid INT interval (-EINVAL)
 *   - More than one packet for INT (-EINVAL)
 */
static void usb_write_bulk_callback(struct urb *urb){
	if(urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET ||urb->status == -ESHUTDOWN))
		pr_debug("%s:  Nonzero write bulk status received: %d",__func__,urb->status);
	/* free up our allocated buffer */
	usb_free_coherent(urb->dev,urb->transfer_buffer_length,urb->transfer_buffer,urb->transfer_dma);

}
static ssize_t usb_write(struct file *filep, const char __user *buffer, size_t count, loff_t *offset){
	struct usb_dev *dev;
	int retval;
	struct urb *urb=NULL;  /* struct urb - USB Request Block*/
	char *buf = NULL;
	dev=(struct usb_dev *)filep->private_data;
	/* verify that we actually have some data to write */
	if (count == 0)
		goto exit;
	/* create a urb, and a buffer for it, and copy the data to the urb.URBs are allocated with the following call*/
	urb=usb_alloc_urb(0,GFP_KERNEL);  /* If the return value is NULL, some error occurred within the USB core*/
	/*p1:iso_packets If the driver want to use this urb for interrupt, control, or bulk endpoints, pass '0' as the number of iso packets. */
	if(!urb){
		retval = -ENOMEM;
		goto error;
	}
	/*usb_buffer_alloc() is renamed to usb_alloc_coherent(), allocate dma-consistent buffer for URB_NO_xxx_DMA_MAP*/
	buf=usb_alloc_coherent(dev->udev,count,GFP_KERNEL,&urb->transfer_dma);/*dma_addr_t transfer_dma;  (in) dma addr for transfer_buffer */
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}
	if (copy_from_user(buf, buffer, count)) {
		retval = -EFAULT;
		goto error;
	}
/**
 * usb_fill_bulk_urb - macro to help initialize a bulk urb
 * @urb: pointer to the urb to initialize.
 * @dev: pointer to the struct usb_device for this urb.
 * @pipe: the endpoint pipe  : usb_sndbulkpipe(dev, endpoint)
 * @transfer_buffer: pointer to the transfer buffer
 * @buffer_length: length of the transfer buffer
 * @complete_fn: pointer to the usb_complete_t function
 * @context: what to set the urb context to.
 *
 * Initializes a bulk urb with the proper information needed to submit it
 * to a device.
 */
      usb_fill_bulk_urb(urb,dev->udev,usb_sndbulkpipe(dev->udev,dev->bulk_out_endpointAddr),buf,count,usb_write_bulk_callback,dev);
	/*set URB_NO_TRANSFER_DMA_MAP so that usbcore won't map or unmap the buffer.*/
      /*If short packets should NOT be tolerated, set URB_SHORT_NOT_OK in transfer_flags.*/
      urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
      /* send the data out the bulk port */
      retval=usb_submit_urb(urb, GFP_KERNEL); //ON submit return 0.
      if(retval){
	      pr_err("%s: failed submitting write urb, error %d",__func__,retval);
	      goto error;
      }
    /* release our reference to this urb, the USB core will eventually free it entirely */
       usb_free_urb(urb);

exit:
	return count;
error:
	usb_free_coherent(dev->udev,count,buf,urb->transfer_dma);
	usb_free_urb(urb);
	kfree(buf);
	return retval;
}
/* * @probe: Called to see if the driver is willing to manage a particular
 *      interface on a device.  If it is, probe returns zero and uses
 *      usb_set_intfdata() to associate driver-specific data with the
 *      interface.  It may also use usb_set_interface() to specify the
 *      appropriate altsetting.  If unwilling to manage the interface,
 *      return -ENODEV, if genuine IO errors occurred, an appropriate
 *      negative errno value. */

static struct file_operations usb_fops = {
	.owner   = THIS_MODULE,
	.read   = usb_read,
	.write  = usb_write,
	.open   = usb_open,
	.release= usb_release,
};
/**
 * struct usb_class_driver - identifies a USB driver that wants to use the USB major number
 * @name: the usb class device name for this driver.  Will show up in sysfs.
 * @devnode: Callback to provide a naming hint for a possible
 *      device node to create.
 * @fops: pointer to the struct file_operations of this driver.
 * @minor_base: the start of the minor range for this driver.
 *
 * This structure is used for the usb_register_dev() and
 * usb_unregister_dev() functions, to consolidate a number of the
 * parameters used for them.
 */
struct usb_class_driver usb_class={
	.name="usbdrv%d"
	.fops=&usb_fops,
	.minor_base = USB_SKEL_MINOR_BASE,
};

static int usb_probe(struct usb_interface *intf,const struct usb_device_id *id){
	   pr_info("USB device  (%04X:%04X) is plugged\n", id->idVendor, id->idProduct);
    	   return 0;
}

/* @disconnect: Called when the interface is no longer accessible, usually
 *      because its device has been (or is being) disconnected or the
 *      driver module is being unloaded.*/
void usb_disconnect(struct usb_interface *intf){
	pr_info("USB device is disconnected\n");
}

struct usb_driver usb_drv={
//	.owner = THIS_MODULE,
	.name="usbdev", 
	.id_table= usb_table,
	.probe= usb_probe,
	.disconnect=usb_disconnect,
};

int __init usb_init(void){
	pr_info("Initialization of USB driver\n");
	usb_register(&usb_drv);
	return 0;
}

void __exit usb_exit(void){
	usb_deregister(&usb_drv);
}

module_init(usb_init);
module_exit(usb_exit);

