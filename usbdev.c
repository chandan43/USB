#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/usb.h>

/*Driver INFO*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("beingchandanjha@gamil.com");
MODULE_DESCRIPTION("My first USB device driver");
MODULE_VERSION(".1");
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

struct usb_driver usb_driver={
//	.owner = THIS_MODULE,
	.name="usbdev", 
	.id_table= usb_table,
	.probe= usb_probe,
	.disconnect=usb_disconnect,
};

int __init usb_init(void){
	pr_info("Initialization of USB driver\n");
	usb_register(&usb_driver);
	return 0;
}

void __exit usb_exit(void){
	usb_deregister(&usb_driver);
}

module_init(usb_init);
module_exit(usb_exit);

