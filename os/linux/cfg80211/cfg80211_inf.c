/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2012, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	cfg80211_inf.c

	Abstract:

	Revision History:
	Who		When			What
	--------	----------		----------------------------------------------
	YF Luo		06-28-2012		Init version
		        12-26-2013		Integration of NXTC
*/
#define RTMP_MODULE_OS

#include "rt_config.h"
#include "rtmp_comm.h"
#include "rt_os_util.h"
#include "rt_os_net.h"

#ifdef RT_CFG80211_SUPPORT

extern INT ApCliAllowToSendPacket(
	struct rtmp_adapter *pAd, struct rtmp_wifi_dev *wdev,
	struct sk_buff *pPacket, u8 *pWcid);

bool CFG80211DRV_OpsChgVirtualInf(struct rtmp_adapter *pAd, VOID *pData)
{
	UINT newType, oldType;
	CMD_RTPRIV_IOCTL_80211_VIF_PARM *pVifParm;
	pVifParm = (CMD_RTPRIV_IOCTL_80211_VIF_PARM *)pData;

	newType = pVifParm->newIfType;
	oldType = pVifParm->oldIfType;

#ifdef CONFIG_STA_SUPPORT
	/* Change Device Type */
	if (newType == RT_CMD_80211_IFTYPE_ADHOC)
	{
		Set_NetworkType_Proc(pAd, "Adhoc");
	}
	else if (newType == RT_CMD_80211_IFTYPE_STATION)
	{
		CFG80211DBG(RT_DEBUG_TRACE, ("80211> Change the Interface to STA Mode\n"));

#ifdef CONFIG_AP_SUPPORT
		if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP && RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP))
			CFG80211DRV_DisableApInterface(pAd);
#endif /* CONFIG_AP_SUPPORT */

		pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_STATION;
	}
	else
#endif /*CONFIG_STA_SUPPORT*/
		if (newType == RT_CMD_80211_IFTYPE_AP)
		{
			CFG80211DBG(RT_DEBUG_TRACE, ("80211> Change the Interface to AP Mode\n"));
			pAd->cfg80211_ctrl.isCfgInApMode = RT_CMD_80211_IFTYPE_AP;
		}
#ifdef CONFIG_STA_SUPPORT
	else if (newType == RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* set packet filter */
		Set_NetworkType_Proc(pAd, "Monitor");

		if (pVifParm->MonFilterFlag != 0)
		{
			uint32_t Filter;

			Filter = mt76u_reg_read(pAd, RX_FILTR_CFG);

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_FCSFAIL) == RT_CMD_80211_FILTER_FCSFAIL)
			{
				Filter = Filter & (~0x01);
			}
			else
			{
				Filter = Filter | 0x01;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_PLCPFAIL) == RT_CMD_80211_FILTER_PLCPFAIL)
			{
				Filter = Filter & (~0x02);
			}
			else
			{
				Filter = Filter | 0x02;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_CONTROL) == RT_CMD_80211_FILTER_CONTROL)
			{
				Filter = Filter & (~0xFF00);
			}
			else
			{
				Filter = Filter | 0xFF00;
			}

			if ((pVifParm->MonFilterFlag & RT_CMD_80211_FILTER_OTHER_BSS) == RT_CMD_80211_FILTER_OTHER_BSS)
			{
				Filter = Filter & (~0x08);
			}
			else
			{
				Filter = Filter | 0x08;
			}

			mt76u_reg_write(pAd, RX_FILTR_CFG, Filter);
			pVifParm->MonFilterFlag = Filter;
		}
	}
#endif /*CONFIG_STA_SUPPORT*/

	return true;
}

bool CFG80211DRV_OpsVifAdd(struct rtmp_adapter *pAd, VOID *pData)
{
	CMD_RTPRIV_IOCTL_80211_VIF_SET *pVifInfo;
	pVifInfo = (CMD_RTPRIV_IOCTL_80211_VIF_SET *)pData;

	/* Already one VIF in list */
	if (pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn)
		return false;

	pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn = true;
	RTMP_CFG80211_VirtualIF_Init(pAd, pVifInfo->vifName, pVifInfo->vifType);
	return true;
}

bool RTMP_CFG80211_VIF_ON(struct rtmp_adapter *pAd)
{
	return pAd->cfg80211_ctrl.Cfg80211VifDevSet.isGoingOn;
}


static
PCFG80211_VIF_DEV RTMP_CFG80211_FindVifEntry_ByMac(struct rtmp_adapter *pAd, struct net_device *pNewNetDev)
{
	PLIST_HEADER  pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV       	pDevEntry = NULL;
	PLIST_ENTRY		        pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	while (pDevEntry != NULL)
	{
		if (RTMPEqualMemory(pDevEntry->net_dev->dev_addr, pNewNetDev->dev_addr, MAC_ADDR_LEN))
			return pDevEntry;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	}

	return NULL;
}

struct net_device *RTMP_CFG80211_FindVifEntry_ByType(struct rtmp_adapter *pAd, uint32_t devType)
{
	PLIST_HEADER  pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV       	pDevEntry = NULL;
	PLIST_ENTRY		        pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	while (pDevEntry != NULL)
	{
		if (pDevEntry->devType == devType)
			return pDevEntry->net_dev;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	}

	return NULL;
}

PWIRELESS_DEV RTMP_CFG80211_FindVifEntryWdev_ByType(
	IN      struct rtmp_adapter *pAd,
	      uint32_t    devType)
{
	PLIST_HEADER  pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV       	pDevEntry = NULL;
	PLIST_ENTRY		        pListEntry = NULL;

	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	while (pDevEntry != NULL)
	{
		if (pDevEntry->devType == devType)
			return pDevEntry->net_dev->ieee80211_ptr;

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	}

	return NULL;
}

VOID RTMP_CFG80211_AddVifEntry(struct rtmp_adapter *pAd, struct net_device *pNewNetDev, uint32_t DevType)
{
	PCFG80211_VIF_DEV pNewVifDev = NULL;

	pNewVifDev = kmalloc(sizeof(CFG80211_VIF_DEV), GFP_ATOMIC);
	if (pNewVifDev) {
		memset(pNewVifDev, 0, sizeof(CFG80211_VIF_DEV));

		pNewVifDev->pNext = NULL;
		pNewVifDev->net_dev = pNewNetDev;
		pNewVifDev->devType = DevType;
		memset(pNewVifDev->CUR_MAC, 0, MAC_ADDR_LEN);
		memcpy(pNewVifDev->CUR_MAC, pNewNetDev->dev_addr, MAC_ADDR_LEN);

		insertTailList(&pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList, (PLIST_ENTRY)pNewVifDev);
		DBGPRINT(RT_DEBUG_TRACE, ("Add CFG80211 VIF Device, Type: %d.\n", pNewVifDev->devType));
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("Error in alloc mem in New CFG80211 VIF Function.\n"));
	}
}

VOID RTMP_CFG80211_RemoveVifEntry(struct rtmp_adapter *pAd, struct net_device *pNewNetDev)
{
	PLIST_ENTRY     pListEntry = NULL;

	pListEntry = (PLIST_ENTRY)RTMP_CFG80211_FindVifEntry_ByMac(pAd, pNewNetDev);

	if (pListEntry)
	{
		delEntryList(&pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList, pListEntry);
		kfree(pListEntry);
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("Error in RTMP_CFG80211_RemoveVifEntry.\n"));
	}
}

struct net_device *RTMP_CFG80211_VirtualIF_Get(
	IN struct rtmp_adapter *pAdSrc
)
{
	//struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdSrc;
	//return pAd->Cfg80211VifDevSet.Cfg80211VifDev[0].net_dev;
	return NULL;
}

static INT CFG80211_VirtualIF_Open(struct net_device *dev_p)
{
	struct rtmp_adapter *pAd;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAd);

	DBGPRINT(RT_DEBUG_TRACE, ("%s: ===> %d,%s\n", __FUNCTION__, dev_p->ifindex,
						RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	//if (VIRTUAL_IF_UP(pAd) != 0)
	//	return -1;

	/* increase MODULE use count */
	RT_MOD_INC_USE_COUNT();

	RTMP_OS_NETDEV_START_QUEUE(dev_p);
	DBGPRINT(RT_DEBUG_TRACE, ("%s: <=== %s\n", __FUNCTION__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	return 0;
}

static INT CFG80211_VirtualIF_Close(struct net_device *dev_p)
{
	struct rtmp_adapter *pAd;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAd);

	DBGPRINT(RT_DEBUG_TRACE, ("%s: ===> %s\n", __FUNCTION__, RTMP_OS_NETDEV_GET_DEVNAME(dev_p)));

	RTMP_OS_NETDEV_STOP_QUEUE(dev_p);

	if (netif_carrier_ok(dev_p))
		netif_carrier_off(dev_p);
#ifdef CONFIG_STA_SUPPORT
	if (INFRA_ON(pAd))
		AsicEnableBssSync(pAd);

	else if (ADHOC_ON(pAd))
		AsicEnableIbssSync(pAd);
#else
	else
		AsicDisableSync(pAd);
#endif

	//VIRTUAL_IF_DOWN(pAd);

	RT_MOD_DEC_USE_COUNT();
	return 0;
}

static INT CFG80211_PacketSend(struct sk_buff *pPktSrc, struct net_device *pDev, RTMP_NET_PACKET_TRANSMIT Func)
{
    	struct rtmp_adapter *pAd;
    	pAd = RTMP_OS_NETDEV_GET_PRIV(pDev);
    	ASSERT(pAd);

	/* To Indicate from Which VIF */
	switch (pDev->ieee80211_ptr->iftype)
	{
		case RT_CMD_80211_IFTYPE_AP:
			//minIdx = MIN_NET_DEVICE_FOR_CFG80211_VIF_AP;
			RTMP_SET_PACKET_OPMODE(pPktSrc, OPMODE_AP);
			break;

		case RT_CMD_80211_IFTYPE_STATION:
		default:
			DBGPRINT(RT_DEBUG_TRACE, ("Unknown CFG80211 I/F Type(%d)\n", pDev->ieee80211_ptr->iftype));
			dev_kfree_skb_any(pPktSrc);
			return 0;
	}

	DBGPRINT(RT_DEBUG_INFO, ("CFG80211 Packet Type  [%s](%d)\n",
					pDev->name, pDev->ieee80211_ptr->iftype));

	RTMP_SET_PACKET_NET_DEVICE_MBSSID(pPktSrc, MAIN_MBSSID);

	return Func(pPktSrc);
}

static INT CFG80211_VirtualIF_PacketSend(
	struct sk_buff *skb, struct net_device *dev_p)
{
	struct rtmp_wifi_dev *wdev;

	DBGPRINT(RT_DEBUG_INFO, ("%s ---> %d\n", __FUNCTION__, dev_p->ieee80211_ptr->iftype));

	if(!(RTMP_OS_NETDEV_STATE_RUNNING(dev_p)))
	{
		/* the interface is down */
		dev_kfree_skb_any(skb);
		return 0;
	}

	/* The device not ready to send packt. */
	wdev = RTMP_OS_NETDEV_GET_WDEV(dev_p);
	ASSERT(wdev);
	if (!wdev) return -1;

	return CFG80211_PacketSend(skb, dev_p, rt28xx_packet_xmit);
}

static INT CFG80211_VirtualIF_Ioctl(struct net_device *dev_p,
				    struct ifreq *ifr, int cmd)
{

	struct rtmp_adapter *pAd;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev_p);
	ASSERT(pAd);

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		return -ENETDOWN;

	DBGPRINT(RT_DEBUG_TRACE, ("%s --->\n", __FUNCTION__));

	return rt28xx_ioctl(dev_p, ifr, cmd);

}

VOID RTMP_CFG80211_VirtualIF_Init(
	IN struct rtmp_adapter	*pAd,
	IN CHAR			*pDevName,
	IN uint32_t                DevType)
{
	struct RTMP_OS_NETDEV_OP_HOOK netDevHook, *pNetDevOps;
	struct net_device *new_dev_p;

	CHAR preIfName[12];
	UINT devNameLen = strlen(pDevName);
	UINT preIfIndex = pDevName[devNameLen-1] - 48;
	struct mt7612u_cfg80211_cb *p80211CB = pAd->pCfg80211_CB;
	struct wireless_dev *pWdev;
	uint32_t MC_RowID = 0, IoctlIF = 0, Inf = INT_P2P;

	memset(preIfName, 0, sizeof(preIfName));
	memcpy(preIfName, pDevName, devNameLen-1);

	pNetDevOps=&netDevHook;

	DBGPRINT(RT_DEBUG_TRACE, ("%s ---> (%s, %s, %d)\n", __FUNCTION__, pDevName, preIfName, preIfIndex));

	/* init operation functions and flags */
	memset(&netDevHook, 0, sizeof(netDevHook));
	netDevHook.open = CFG80211_VirtualIF_Open;	     /* device opem hook point */
	netDevHook.stop = CFG80211_VirtualIF_Close;	     /* device close hook point */
	netDevHook.xmit = CFG80211_VirtualIF_PacketSend; /* hard transmit hook point */
	netDevHook.ioctl = CFG80211_VirtualIF_Ioctl;	 /* ioctl hook point */

	new_dev_p = RtmpOSNetDevCreate(MC_RowID, &IoctlIF, Inf, preIfIndex, sizeof(struct rtmp_adapter *), preIfName);

	if (new_dev_p == NULL)
	{
		/* allocation fail, exit */
		DBGPRINT(RT_DEBUG_ERROR, ("Allocate network device fail (CFG80211)...\n"));
		return;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Register CFG80211 I/F (%s)\n", RTMP_OS_NETDEV_GET_DEVNAME(new_dev_p)));
	}

	new_dev_p->needs_free_netdev = true;
	RTMP_OS_NETDEV_SET_PRIV(new_dev_p, pAd);
	pNetDevOps->needProtcted = true;

	memmove(&pNetDevOps->devAddr[0], &pAd->CurrentAddress[0], MAC_ADDR_LEN);

	//CFG_TODO
	/*
		Bit1 of MAC address Byte0 is local administration bit
		and should be set to 1 in extended multiple BSSIDs'
		Bit3~ of MAC address Byte0 is extended multiple BSSID index.
	*/
	if (pAd->chipCap.MBSSIDMode == MBSSID_MODE1)
		pNetDevOps->devAddr[0] += 2; /* NEW BSSID */
	else
	{
		pNetDevOps->devAddr[5] += FIRST_MBSSID;
	}

	switch (DevType)
	{
		case RT_CMD_80211_IFTYPE_MONITOR:
			DBGPRINT(RT_DEBUG_ERROR, ("CFG80211 I/F Monitor Type\n"));

			//RTMP_OS_NETDEV_SET_TYPE_MONITOR(new_dev_p);
			break;
		default:
			DBGPRINT(RT_DEBUG_ERROR, ("Unknown CFG80211 I/F Type (%d)\n", DevType));
	}

	pWdev = kzalloc(sizeof(*pWdev), GFP_KERNEL);

	new_dev_p->ieee80211_ptr = pWdev;
	pWdev->wiphy = p80211CB->pCfg80211_Wdev->wiphy;
	SET_NETDEV_DEV(new_dev_p, wiphy_dev(pWdev->wiphy));
	pWdev->netdev = new_dev_p;
	pWdev->iftype = DevType;

	RtmpOSNetDevAttach(pAd->OpMode, new_dev_p, pNetDevOps);

	AsicSetBssid(pAd, pAd->CurrentAddress);

	/* Record the pNetDevice to Cfg80211VifDevList */
	RTMP_CFG80211_AddVifEntry(pAd, new_dev_p, DevType);

	DBGPRINT(RT_DEBUG_TRACE, ("%s <---\n", __FUNCTION__));
}

VOID RTMP_CFG80211_VirtualIF_Remove(
	IN  struct rtmp_adapter			 *pAd,
	IN	struct net_device *		  dev_p,
	IN  uint32_t                DevType)
{
	if (dev_p)
	{
		RTMP_CFG80211_RemoveVifEntry(pAd, dev_p);
                RTMP_OS_NETDEV_STOP_QUEUE(dev_p);
		{
			/* Never Opened When New Netdevice on */
			RtmpOSNetDevDetach(dev_p);
		}

		if (dev_p->ieee80211_ptr)
		{
			kfree(dev_p->ieee80211_ptr);
			dev_p->ieee80211_ptr = NULL;
		}
	}
}

VOID RTMP_CFG80211_AllVirtualIF_Remove(
	IN struct rtmp_adapter 		*pAd)
{
	PLIST_HEADER  pCacheList = &pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList;
	PCFG80211_VIF_DEV           pDevEntry = NULL;
	PLIST_ENTRY             pListEntry = NULL;
	pListEntry = pCacheList->pHead;
	pDevEntry = (PCFG80211_VIF_DEV)pListEntry;

	while ((pDevEntry != NULL) && (pCacheList->size != 0))
	{
		RtmpOSNetDevProtect(1);
		RTMP_CFG80211_VirtualIF_Remove(pAd, pDevEntry->net_dev, pDevEntry->net_dev->ieee80211_ptr->iftype);
		RtmpOSNetDevProtect(0);

		pListEntry = pListEntry->pNext;
		pDevEntry = (PCFG80211_VIF_DEV)pListEntry;
	}
}
#endif /* RT_CFG80211_SUPPORT */
