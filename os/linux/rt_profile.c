/****************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ****************************************************************************

    Module Name:
	rt_profile.c

    Abstract:

    Revision History:
    Who          When          What
    ---------    ----------    ----------------------------------------------
 */

#include "rt_config.h"

static char *RT2870STA_dat =
"#The word of \"Default\" must not be removed\n"
"Default\n"
"CountryRegion=5\n"
"CountryRegionABand=7\n"
"CountryCode=\n"
"ChannelGeography=1\n"
"NetworkType=Infra\n"
"WirelessMode=13\n"
"EfuseBufferMode=0\n"
"Channel=0\n"
"BeaconPeriod=100\n"
"TxPower=100\n"
"BGProtection=0\n"
"TxPreamble=0\n"
"RTSThreshold=2347\n"
"FragThreshold=2346\n"
"TxBurst=1\n"
"PktAggregate=0\n"
"WmmCapable=0\n"
"AckPolicy=0;0;0;0\n"
"AuthMode=OPEN\n"
"EncrypType=NONE\n"
"WPAPSK=\n"
"DefaultKeyID=1\n"
"Key1Type=0\n"
"Key1Str=\n"
"Key2Type=0\n"
"Key2Str=\n"
"Key3Type=0\n"
"Key3Str=\n"
"Key4Type=0\n"
"Key4Str=\n"
"PSMode=CAM\n"
"AutoRoaming=0\n"
"RoamThreshold=70\n"
"APSDCapable=0\n"
"APSDAC=0;0;0;0\n"
"HT_RDG=1\n"
"HT_EXTCHA=0\n"
"HT_OpMode=0\n"
"HT_MpduDensity=4\n"
"HT_BW=1\n"
"HT_BADecline=0\n"
"HT_AutoBA=1\n"
"HT_AMSDU=0\n"
"HT_BAWinSize=64\n"
"HT_GI=1\n"
"HT_MCS=33\n"
"HT_MIMOPSMode=3\n"
"HT_DisallowTKIP=1\n"
"HT_STBC=0\n"
"VHT_BW=1\n"
"VHT_SGI=1\n"
"VHT_STBC=0\n"
"EthConvertMode=\n"
"EthCloneMac=\n"
"IEEE80211H=0\n"
"TGnWifiTest=0\n"
"WirelessEvent=0\n"
"MeshId=MESH\n"
"MeshAutoLink=1\n"
"MeshAuthMode=OPEN\n"
"MeshEncrypType=NONE\n"
"MeshWPAKEY=\n"
"MeshDefaultkey=1\n"
"MeshWEPKEY=\n"
"CarrierDetect=0\n"
"AntDiversity=0\n"
"BeaconLostTime=4\n"
"FtSupport=0\n"
"Wapiifname=ra0\n"
"WapiPsk=\n"
"WapiPskType=\n"
"WapiUserCertPath=\n"
"WapiAsCertPath=\n"
"PSP_XLINK_MODE=0\n"
"WscManufacturer=\n"
"WscModelName=\n"
"WscDeviceName=\n"
"WscModelNumber=\n"
"WscSerialNumber=\n"
"RadioOn=1\n"
"WIDIEnable=1\n"
"P2P_L2SD_SCAN_TOGGLE=3\n"
"Wsc4digitPinCode=0\n"
"P2P_WIDIEnable=0\n"
"PMFMFPC=0\n"
"PMFMFPR=0\n"
"PMFSHA256=0\n";


struct dev_type_name_map{
	INT type;
	char *prefix[2];
};


#define SECOND_INF_MAIN_DEV_NAME		"rai"
#define SECOND_INF_MBSSID_DEV_NAME	"rai"
#define SECOND_INF_WDS_DEV_NAME		"wdsi"
#define SECOND_INF_APCLI_DEV_NAME	"apclii"
#define SECOND_INF_MESH_DEV_NAME		"meshi"
#define SECOND_INF_P2P_DEV_NAME		"p2pi"
#define SECOND_INF_MONITOR_DEV_NAME		"moni"


#define xdef_to_str(s)   def_to_str(s)
#define def_to_str(s)    #s

static struct dev_type_name_map prefix_map[] =
{
	{INT_MAIN, 		{INF_MAIN_DEV_NAME, SECOND_INF_MAIN_DEV_NAME}},
#ifdef CONFIG_AP_SUPPORT
#ifdef MBSS_SUPPORT
	{INT_MBSSID, 	{INF_MBSSID_DEV_NAME, SECOND_INF_MBSSID_DEV_NAME}},
#endif /* MBSS_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */

	{0},
};


struct dev_id_name_map{
	INT chip_id;
	char *chip_name;
};

static const struct dev_id_name_map id_name_list[]=
{
	{7610, "7610, 7610e 7610u"},

};

VOID get_dev_config_idx(struct rtmp_adapter *pAd)
{
	INT idx = 0;

	pAd->dev_idx = idx;
}


u8 *get_dev_name_prefix(struct rtmp_adapter *pAd, INT dev_type)
{
	struct dev_type_name_map *map;
	INT type_idx = 0, dev_idx = pAd->dev_idx;

	do {
		map = &prefix_map[type_idx];
		if (map->type == dev_type) {
			DBGPRINT(RT_DEBUG_TRACE, ("%s(): dev_idx = %d, dev_name_prefix=%s\n",
						__FUNCTION__, dev_idx, map->prefix[dev_idx]));
			return map->prefix[dev_idx];
		}
		type_idx++;
	} while (prefix_map[type_idx].type != 0);

	return NULL;
}

int RTMPReadParametersHook(struct rtmp_adapter *pAd)
{
	INT retval = NDIS_STATUS_FAILURE;
	ULONG buf_size = MAX_INI_BUFFER_SIZE;
	char *buffer = NULL;

#ifdef HOSTAPD_SUPPORT
	int i;
#endif /*HOSTAPD_SUPPORT */

		{
			buffer = kmalloc(MAX_INI_BUFFER_SIZE, GFP_ATOMIC);
			if (buffer) {
				memset(buffer, 0x00, buf_size);

				strcpy(buffer, RT2870STA_dat);
				RTMPSetProfileParameters(pAd, buffer);
				kfree(buffer);
				retval = NDIS_STATUS_SUCCESS;
			} else
				retval = NDIS_STATUS_FAILURE;

		}

//		RtmpOSFSInfoChange(&osFSInfo, false);

#ifdef HOSTAPD_SUPPORT
		for (i = 0; i < pAd->ApCfg.BssidNum; i++)
		{
			pAd->ApCfg.MBSSID[i].Hostapd=Hostapd_Diable;
			DBGPRINT(RT_DEBUG_TRACE, ("Reset ra%d hostapd support=FLASE", i));
		}
#endif /*HOSTAPD_SUPPORT */

	return (retval);

}


void RTMP_IndicateMediaState(
	IN	struct rtmp_adapter *	pAd,
	IN  NDIS_MEDIA_STATE	media_state)
{
	pAd->IndicateMediaState = media_state;

#ifdef SYSTEM_LOG_SUPPORT
		if (pAd->IndicateMediaState == NdisMediaStateConnected)
		{
			RTMPSendWirelessEvent(pAd, IW_STA_LINKUP_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
		}
		else
		{
			RTMPSendWirelessEvent(pAd, IW_STA_LINKDOWN_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
		}
#endif /* SYSTEM_LOG_SUPPORT */
}


void tbtt_tasklet(unsigned long data)
{
#ifdef CONFIG_AP_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)data;

	if (pAd->OpMode == OPMODE_AP)
	{
		/* step 7 - if DTIM, then move backlogged bcast/mcast frames from PSQ to TXQ whenever DtimCount==0 */
		if ((pAd->ApCfg.DtimCount + 1) == pAd->ApCfg.DtimPeriod)
		{
			QUEUE_ENTRY *pEntry;
			bool bPS = false;
			UINT count = 0;

			spin_lock_bh(&pAd->irq_lock);
			while (pAd->MacTab.McastPsQueue.Head)
			{
				bPS = true;
				if (pAd->TxSwQueue[QID_AC_BE].Number <= (pAd->TxSwQMaxLen + MAX_PACKETS_IN_MCAST_PS_QUEUE))
				{
					struct sk_buff *skb;

					pEntry = RemoveHeadQueue(&pAd->MacTab.McastPsQueue);
					skb = QUEUE_ENTRY_TO_PACKET(pEntry);

					/*if(pAd->MacTab.McastPsQueue.Number) */
					if (count)
					{
						RTMP_SET_PACKET_MOREDATA(skb, true);
					}
					InsertHeadQueue(&pAd->TxSwQueue[QID_AC_BE], pEntry);
					count++;
				}
				else
				{
					break;
				}
			}
			spin_unlock_bh(&pAd->irq_lock);


			if (pAd->MacTab.McastPsQueue.Number == 0)
			{
		                UINT bss_index;

                		/* clear MCAST/BCAST backlog bit for all BSS */
				for(bss_index=BSS0; bss_index<pAd->ApCfg.BssidNum; bss_index++)
					WLAN_MR_TIM_BCMC_CLEAR(bss_index);
			}
			pAd->MacTab.PsQIdleCount = 0;

			if (bPS == true)
			{
				RTMPDeQueuePacket(pAd, false, NUM_OF_TX_RING, /*MAX_TX_IN_TBTT*/MAX_PACKETS_IN_MCAST_PS_QUEUE);
			}
		}
	}
#endif /* CONFIG_AP_SUPPORT */
}

void announce_802_3_packet(
	IN struct rtmp_adapter *pAd,
	IN struct sk_buff *pPacket,
	IN u8 OpMode)
{
	struct sk_buff *pRxPkt = pPacket;


	ASSERT(pPacket);

	{
#ifdef CONFIG_RT2880_BRIDGING_ONLY
		PACKET_CB_ASSIGN(pRxPkt, 22) = 0xa8;
#endif

#if defined(CONFIG_RA_CLASSIFIER)||defined(CONFIG_RA_CLASSIFIER_MODULE)
		if(ra_classifier_hook_rx!= NULL)
		{
			unsigned int flags;

			spin_lock_bh(&pAd->page_lock);
			ra_classifier_hook_rx(pRxPkt, classifier_cur_cycle);
			spin_unlock_bh(&pAd->page_lock);
		}
#endif /* CONFIG_RA_CLASSIFIER */

	}

		RtmpOsPktProtocolAssign(pRxPkt);

		RtmpOsPktRcvHandle(pRxPkt);
}




extern spinlock_t TimerSemLock;

VOID RTMPFreeAdapter(struct rtmp_adapter *pAd)
{
	struct os_cookie *os_cookie;
	int index;

	os_cookie = pAd->OS_Cookie;

	if (pAd->BeaconBuf)
		kfree(pAd->BeaconBuf);

	for (index =0 ; index < NUM_OF_TX_RING; index++)
	{
		pAd->DeQueueRunning[index] = false;
	}

	if (pAd->iw_stats)
	{
		kfree(pAd->iw_stats);
		pAd->iw_stats = NULL;
	}
	if (pAd->stats)
	{
		kfree(pAd->stats);
		pAd->stats = NULL;
	}

	RTMP_OS_FREE_TIMER(pAd);
	RTMP_OS_FREE_LOCK(pAd);
	RTMP_OS_FREE_TASKLET(pAd);
	RTMP_OS_FREE_TASK(pAd);
	RTMP_OS_FREE_SEM(pAd);
	RTMP_OS_FREE_ATOMIC(pAd);

	RtmpOsVfree(pAd); /* pci_free_consistent(os_cookie->pci_dev,sizeof(struct rtmp_adapter),pAd,os_cookie->pAd_pa); */
	if (os_cookie)
		kfree(os_cookie);
}




int RTMPSendPackets(
	IN NDIS_HANDLE dev_hnd,
	IN struct sk_buff **pkt_list,
	IN UINT pkt_cnt,
	IN uint32_t pkt_total_len,
	IN RTMP_NET_ETH_CONVERT_DEV_SEARCH Func)
{
	struct rtmp_wifi_dev *wdev = (struct rtmp_wifi_dev *)dev_hnd;
	struct rtmp_adapter *pAd;
	struct sk_buff *pPacket = pkt_list[0];

	ASSERT(wdev->sys_handle);
	pAd = (struct rtmp_adapter *)wdev->sys_handle;
	INC_COUNTER64(pAd->WlanCounters.TransmitCountFrmOs);

	if (!pPacket)
		return 0;


	if (pkt_total_len < 14)
	{
		hex_dump("bad packet", pPacket->data, pkt_total_len);
		dev_kfree_skb_any(pPacket);
		return 0;
	}

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		/* Drop send request since we are in monitor mode */
		if (MONITOR_ON(pAd))
		{
			dev_kfree_skb_any(pPacket);
			return 0;
		}
	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_5VT_ENHANCE
	RTMP_SET_PACKET_5VT(pPacket, 0);
	if (*(int*)(GET_OS_PKT_CB(pPacket)) == BRIDGE_TAG) {
		RTMP_SET_PACKET_5VT(pPacket, 1);
	}
#endif /* CONFIG_5VT_ENHANCE */

#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */

	wdev_tx_pkts((NDIS_HANDLE)pAd, &pPacket, 1, wdev);

#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */
	return 0;
}


struct net_device *get_netdev_from_bssid(struct rtmp_adapter *pAd, u8 FromWhichBSSID)
{
	struct net_device *dev_p = NULL;
#ifdef CONFIG_AP_SUPPORT
	u8 infRealIdx;
#endif /* CONFIG_AP_SUPPORT */

	do
	{
#ifdef CONFIG_AP_SUPPORT
		infRealIdx = FromWhichBSSID & (NET_DEVICE_REAL_IDX_MASK);

		if ((FromWhichBSSID > 0) &&
			(FromWhichBSSID < pAd->ApCfg.BssidNum) &&
				(FromWhichBSSID < MAX_MBSSID_NUM(pAd)) &&
				(FromWhichBSSID < HW_BEACON_MAX_NUM))
    		{
    	    		dev_p = pAd->ApCfg.MBSSID[FromWhichBSSID].wdev.if_dev;
    		}
		else
#endif /* CONFIG_AP_SUPPORT */
		{
			dev_p = pAd->net_dev;
		}

	} while (false);

	ASSERT(dev_p);
	return dev_p;
}


#ifdef CONFIG_AP_SUPPORT
/*
========================================================================
Routine Description:
	Driver pre-Ioctl for AP.

Arguments:
	pAdSrc			- WLAN control block pointer
	pCB				- the IOCTL parameters

Return Value:
	NDIS_STATUS_SUCCESS	- IOCTL OK
	Otherwise			- IOCTL fail

Note:
========================================================================
*/
INT RTMP_AP_IoctlPrepare(struct rtmp_adapter *pAd, VOID *pCB)
{
	RT_CMD_AP_IOCTL_CONFIG *pConfig = (RT_CMD_AP_IOCTL_CONFIG *)pCB;
	struct os_cookie *pObj;
	unsigned short index;
	INT	Status = NDIS_STATUS_SUCCESS;

	pObj =  pAd->OS_Cookie;

	if((pConfig->priv_flags == INT_MAIN) && !RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		if (pConfig->pCmdData == NULL)
			return Status;

		if (RtPrivIoctlSetVal() == pConfig->CmdId_RTPRIV_IOCTL_SET)
		{
			if (true
#ifdef CONFIG_APSTA_MIXED_SUPPORT
				&& (strstr(pConfig->pCmdData, "OpMode") == NULL)
#endif /* CONFIG_APSTA_MIXED_SUPPORT */
			)
			{
				return -ENETDOWN;
			}
		}
		else
			return -ENETDOWN;
    }

    /* determine this ioctl command is comming from which interface. */
    if (pConfig->priv_flags == INT_MAIN)
    {
		pObj->ioctl_if_type = INT_MAIN;
		pObj->ioctl_if = MAIN_MBSSID;
    }
    else if (pConfig->priv_flags == INT_MBSSID)
    {
		pObj->ioctl_if_type = INT_MBSSID;
/*    	if (!RTMPEqualMemory(net_dev->name, pAd->net_dev->name, 3))  // for multi-physical card, no MBSSID */
		if (strcmp(pConfig->name, RtmpOsGetNetDevName(pAd->net_dev)) != 0) /* sample */
    	{
	        for (index = 1; index < pAd->ApCfg.BssidNum; index++)
	    	{
	    	    if (pAd->ApCfg.MBSSID[index].wdev.if_dev == pConfig->net_dev)
	    	    {
	    	        pObj->ioctl_if = index;
	    	        break;
	    	    }
	    	}
	        /* Interface not found! */
	        if(index == pAd->ApCfg.BssidNum)
	            return -ENETDOWN;
	    }
	    else    /* ioctl command from I/F(ra0) */
	    {
    	    pObj->ioctl_if = MAIN_MBSSID;
	    }
        MBSS_MR_APIDX_SANITY_CHECK(pAd, pObj->ioctl_if);
    }
    else
    {
	/* DBGPRINT(RT_DEBUG_WARN, ("IOCTL is not supported in WDS interface\n")); */
    	return -EOPNOTSUPP;
    }

	pConfig->apidx = pObj->ioctl_if;
	return Status;
}


VOID AP_E2PROM_IOCTL_PostCtrl(
	IN	RTMP_IOCTL_INPUT_STRUCT	*wrq,
	IN	char *				msg)
{
	wrq->u.data.length = strlen(msg);
	if (copy_to_user(wrq->u.data.pointer, msg, wrq->u.data.length))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s: copy_to_user() fail\n", __FUNCTION__));
	}
}


VOID IAPP_L2_UpdatePostCtrl(
	IN struct rtmp_adapter *pAd,
    IN UINT8 *mac_p,
    IN INT  bssid)
{
}
#endif /* CONFIG_AP_SUPPORT */



