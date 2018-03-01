/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"

#define RTMP_DRV_NAME		"mt7612u"

extern USB_DEVICE_ID rtusb_dev_id[];

static bool USBDevConfigInit(struct usb_device *dev, struct usb_interface *intf, struct rtmp_adapter *pAd)
{
	struct usb_host_interface *iface_desc;
	ULONG BulkOutIdx;
	ULONG BulkInIdx;
	uint32_t i;

	/* get the active interface descriptor */
	iface_desc = intf->cur_altsetting;

	/* get # of enpoints  */
	DBGPRINT(RT_DEBUG_TRACE, ("NumEndpoints=%d\n", iface_desc->desc.bNumEndpoints));

	/* Configure Pipes */
	BulkOutIdx = 0;
	BulkInIdx = 0;

	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
			((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN)) {
			if (BulkInIdx < 2) {
				pAd->in_eps[BulkInIdx++] = iface_desc->endpoint[i].desc.bEndpointAddress;
				pAd->in_max_packet = le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);

				DBGPRINT_RAW(RT_DEBUG_TRACE, ("BULK IN MaxPacketSize = %d\n", pAd->in_max_packet));
				DBGPRINT_RAW(RT_DEBUG_TRACE, ("EP address = 0x%2x\n", iface_desc->endpoint[i].desc.bEndpointAddress));
			} else {
					DBGPRINT(RT_DEBUG_ERROR, ("Bulk IN endpoint nums large than 2\n"));
			}
		} else if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) &&
			   ((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT)) {
			if (BulkOutIdx < 6) {
			/* there are 6 bulk out EP. EP6 highest priority. */
			/* EP1-4 is EDCA.  EP5 is HCCA. */
			pAd->out_eps[BulkOutIdx++] = iface_desc->endpoint[i].desc.bEndpointAddress;
			pAd->out_max_packet = le2cpu16(iface_desc->endpoint[i].desc.wMaxPacketSize);

			DBGPRINT_RAW(RT_DEBUG_TRACE, ("BULK OUT MaxPacketSize = %d\n", pAd->out_max_packet));
			DBGPRINT_RAW(RT_DEBUG_TRACE, ("EP address = 0x%2x  \n", iface_desc->endpoint[i].desc.bEndpointAddress));
			} else {
				DBGPRINT(RT_DEBUG_ERROR, ("Bulk Out endpoint nums large than 6\n"));
			}
		}
	}

	if (!(pAd->in_eps && pAd->out_eps[0])) {
		printk("%s: Could not find both bulk-in and bulk-out endpoints\n", __FUNCTION__);
		return false;
	}

	usb_set_intfdata(intf, pAd);

	return true;

}


static int rt2870_probe(
	struct usb_interface *intf,
	struct usb_device *usb_dev,
	const USB_DEVICE_ID *dev_id)
{
	struct net_device *net_dev = NULL;
	struct rtmp_adapter *pAd = NULL;
	INT status, rv;
	PVOID handle;
	struct RTMP_OS_NETDEV_OP_HOOK netDevHook;
	ULONG OpMode;

	DBGPRINT(RT_DEBUG_TRACE, ("===>rt2870_probe()!\n"));

	handle = kzalloc(sizeof(struct os_cookie), GFP_ATOMIC);
	if (handle == NULL) {
		printk("rt2870_probe(): Allocate memory for os handle failed!\n");
		return -ENOMEM;
	}

	((struct os_cookie *)handle)->pUsb_Dev = usb_dev;


	/* set/get operators to/from DRIVER module */

	rv = RTMPAllocAdapterBlock(handle, &pAd);
	if (rv != NDIS_STATUS_SUCCESS)
	{
		kfree(handle);
		goto err_out;
	}

	if (USBDevConfigInit(usb_dev, intf, pAd) == false)
		goto err_out_free_radev;

	InitUSBDevice(pAd);

	net_dev = RtmpPhyNetDevInit(pAd, &netDevHook);
	if (net_dev == NULL)
		goto err_out_free_radev;

	/* Here are the net_device structure with usb specific parameters. */
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	/* for supporting Network Manager.
	  * Set the sysfs physical device reference for the network logical device if set prior to registration will
	  * cause a symlink during initialization.
	 */
	SET_NETDEV_DEV(net_dev, &(usb_dev->dev));
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
	pAd->StaCfg.OriDevType = net_dev->type;
#endif /* CONFIG_STA_SUPPORT */

/*All done, it's time to register the net device to linux kernel. */
	/* Register this device */
	CFG80211_Register(pAd, &(usb_dev->dev), net_dev);

	RTMP_DRIVER_OP_MODE_GET(pAd, &OpMode);
	status = RtmpOSNetDevAttach(OpMode, net_dev, &netDevHook);
	if (status != 0)
		goto err_out_free_netdev;

	DBGPRINT(RT_DEBUG_TRACE, ("<===rt2870_probe()!\n"));

	return 0;

	/* --------------------------- ERROR HANDLE --------------------------- */
err_out_free_netdev:
	RtmpOSNetDevFree(net_dev);

err_out_free_radev:
	RTMPFreeAdapter(pAd);

err_out:

	return -1;

}


/*
========================================================================
Routine Description:
    Release allocated resources.

Arguments:
    *dev				Point to the PCI or USB device
	pAd					driver control block pointer

Return Value:
    None

Note:
========================================================================
*/
static void rt2870_disconnect(struct usb_device *dev, struct rtmp_adapter *pAd)
{
	struct net_device *net_dev;


	DBGPRINT(RT_DEBUG_ERROR, ("rtusb_disconnect: unregister usbnet usb-%s-%s\n",
				dev->bus->bus_name, dev->devpath));
	if (!pAd)
	{
		usb_put_dev(dev);

		printk("rtusb_disconnect: pAd == NULL!\n");
		return;
	}
/*	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST); */
	RTMP_DRIVER_NIC_NOT_EXIST_SET(pAd);

	/* for debug, wait to show some messages to /proc system */
	udelay(1);


	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);

	RtmpPhyNetDevExit(pAd, net_dev);

	/* FIXME: Shall we need following delay and flush the schedule?? */
	udelay(1);
	flush_scheduled_work();
	udelay(1);

	RTMP_DRIVER_80211_UNREGISTER(pAd, net_dev);

	RtmpRaDevCtrlExit(pAd);

	/* free the root net_device */
	RtmpOSNetDevFree(net_dev);

	/* release a use of the usb device structure */
	usb_put_dev(dev);
	udelay(1);

	DBGPRINT(RT_DEBUG_ERROR, (" RTUSB disconnect successfully\n"));
}

/**************************************************************************/
/**************************************************************************/
/*tested for kernel 2.6series */
/**************************************************************************/
/**************************************************************************/

#ifdef CONFIG_PM

static int rtusb_suspend(struct usb_interface *intf, pm_message_t state)
{
	struct net_device *net_dev;
	VOID *pAd = usb_get_intfdata(intf);



	DBGPRINT(RT_DEBUG_TRACE, ("%s()=>\n", __FUNCTION__));
/*	net_dev = pAd->net_dev; */
	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
	netif_device_detach(net_dev);

	RTMP_DRIVER_USB_SUSPEND(pAd, netif_running(net_dev));
	DBGPRINT(RT_DEBUG_TRACE, ("<=%s()\n", __FUNCTION__));
	return 0;
}


static int rtusb_resume(struct usb_interface *intf)
{
	struct net_device *net_dev;
	VOID *pAd = usb_get_intfdata(intf);



	DBGPRINT(RT_DEBUG_TRACE, ("%s()=>\n", __FUNCTION__));

/*	pAd->PM_FlgSuspend = 0; */
	RTMP_DRIVER_USB_RESUME(pAd);

/*	net_dev = pAd->net_dev; */
	RTMP_DRIVER_NET_DEV_GET(pAd, &net_dev);
	netif_device_attach(net_dev);
	netif_start_queue(net_dev);
	netif_carrier_on(net_dev);
	netif_wake_queue(net_dev);

	DBGPRINT(RT_DEBUG_TRACE, ("<=%s()\n", __FUNCTION__));
	return 0;
}
#endif /* CONFIG_PM */


static int rtusb_probe(struct usb_interface *intf, const USB_DEVICE_ID *id)
{
	struct usb_device *dev;
	int rv;

	dev = interface_to_usbdev(intf);
	dev = usb_get_dev(dev);

	rv = rt2870_probe(intf, dev, id);
	if (rv != 0)
		usb_put_dev(dev);
	return rv;
}


static void rtusb_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct rtmp_adapter *pAd;

	pAd = usb_get_intfdata(intf);
	usb_set_intfdata(intf, NULL);

	rt2870_disconnect(dev, pAd);


}


struct usb_driver rtusb_driver = {
	.name = RTMP_DRV_NAME,
	.probe = rtusb_probe,
	.disconnect = rtusb_disconnect,
	.id_table = rtusb_dev_id,

};


INT __init rtusb_init(void)
{
	printk("rtusb init %s --->\n", RTMP_DRV_NAME);
	return usb_register(&rtusb_driver);
}


VOID __exit rtusb_exit(void)
{
	usb_deregister(&rtusb_driver);
	printk("<--- rtusb exit\n");
}

module_init(rtusb_init);
module_exit(rtusb_exit);

/* Following information will be show when you run 'modinfo' */
/* *** If you have a solution for the bug in current version of driver, please mail to me. */
/* Otherwise post to forum in ralinktech's web site(www.ralinktech.com) and let all users help you. *** */
MODULE_AUTHOR("Ralink");
MODULE_DESCRIPTION("Ralink Wireless Lan Linux Driver");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE(MT7662U_FIRMWARE_NAME);
MODULE_FIRMWARE(MT7662U_FIRMWARE_PATCH_NAME);

#ifdef CONFIG_STA_SUPPORT
#ifdef MODULE_VERSION
MODULE_VERSION(STA_DRIVER_VERSION);
#endif
#endif /* CONFIG_STA_SUPPORT */

