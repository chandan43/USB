#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/usb.h>
#include <linux/kref.h>
#include <linux/types.h>
#include <linux/errno.h>
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
/* * @probe: Called to see if the driver is willing to manage a particular
 *      interface on a device.  If it is, probe returns zero and uses
 *      usb_set_intfdata() to associate driver-specific data with the
 *      interface.  It may also use usb_set_interface() to specify the
 *      appropriate altsetting.  If unwilling to manage the interface,
 *      return -ENODEV, if genuine IO errors occurred, an appropriate
 *      negative errno value. */


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

