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

/****************************************************************************

	Abstract:

	All related CFG80211 function body.

	History:

***************************************************************************/
#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      struct rtmp_adapter                    *pAd,
	IN      VOID                            *pData,
	IN      ULONG                            Data);

extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      struct rtmp_adapter                    *pAd,
	IN      VOID                            *pData,
	IN      ULONG                            Data);


INT CFG80211DRV_IoctlHandle(
	IN	struct rtmp_adapter			*pAd,
	IN	RTMP_IOCTL_INPUT_STRUCT	*wrq,
	IN	INT						cmd,
	IN	unsigned short 				subcmd,
	IN	VOID					*pData,
	IN	ULONG					Data)
{
	switch(cmd)
	{
		case CMD_RTPRIV_IOCTL_80211_START:
		case CMD_RTPRIV_IOCTL_80211_END:
			/* nothing to do */
			break;

		case CMD_RTPRIV_IOCTL_80211_CHAN_SET:
			if (CFG80211DRV_OpsSetChannel(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_VIF_CHG:
			if (CFG80211DRV_OpsChgVirtualInf(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_SCAN:
			if (CFG80211DRV_OpsScanCheckStatus(pAd, Data) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_SCAN_STATUS_LOCK_INIT:
			CFG80211_ScanStatusLockInit(pAd, Data);
			break;

		case CMD_RTPRIV_IOCTL_80211_IBSS_JOIN:
			CFG80211DRV_OpsJoinIbss(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_STA_LEAVE:
			CFG80211DRV_OpsLeave(pAd, Data);
			break;

		case CMD_RTPRIV_IOCTL_80211_STA_GET:
			if (CFG80211DRV_StaGet(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;
		case CMD_RTPRIV_IOCTL_80211_STA_KEY_ADD:
			CFG80211DRV_StaKeyAdd(pAd, pData);
			break;

#ifdef CONFIG_STA_SUPPORT
		case CMD_RTPRIV_IOCTL_80211_STA_KEY_DEFAULT_SET:
			CFG80211_setStaDefaultKey(pAd, Data);
			break;


#endif /*CONFIG_STA_SUPPORT*/
		case CMD_RTPRIV_IOCTL_80211_CONNECT_TO:
			CFG80211DRV_Connect(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_UNREGISTER:
			CFG80211_UnRegister(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_BANDINFO_GET:
		{
			CFG80211_BAND *pBandInfo = (CFG80211_BAND *)pData;
			CFG80211_BANDINFO_FILL(pAd, pBandInfo);
		}
			break;

		case CMD_RTPRIV_IOCTL_80211_SURVEY_GET:
			CFG80211DRV_SurveyGet(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_EXTRA_IES_SET:
			CFG80211DRV_OpsScanExtraIesSet(pAd);
			break;

		/* CFG_TODO */
		case CMD_RTPRIV_IOCTL_80211_MGMT_FRAME_REG:
			CFG80211DRV_OpsMgmtFrameProbeRegister(pAd, pData, Data);
			break;

		/* CFG_TODO */
		case CMD_RTPRIV_IOCTL_80211_ACTION_FRAME_REG:
			CFG80211DRV_OpsMgmtFrameActionRegister(pAd, pData, Data);
			break;

		case CMD_RTPRIV_IOCTL_80211_CHANNEL_LOCK:
			CFG80211_SwitchTxChannel(pAd, Data);
			break;

		case CMD_RTPRIV_IOCTL_80211_CHANNEL_RESTORE:
			break;

		case CMD_RTPRIV_IOCTL_80211_MGMT_FRAME_SEND:
			CFG80211_SendMgmtFrame(pAd, pData, Data);
			break;

		case CMD_RTPRIV_IOCTL_80211_CHANNEL_LIST_SET:
			return CFG80211DRV_OpsScanSetSpecifyChannel(pAd,pData, Data);

#ifdef CONFIG_AP_SUPPORT
		case CMD_RTPRIV_IOCTL_80211_BEACON_SET:
			CFG80211DRV_OpsBeaconSet(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_BEACON_ADD:
			CFG80211DRV_OpsBeaconAdd(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_BEACON_DEL:
		{
			INT i;
			for(i = 0; i < WLAN_MAX_NUM_OF_TIM; i++)
                		pAd->ApCfg.MBSSID[MAIN_MBSSID].TimBitmaps[i] = 0;
			if (pAd->cfg80211_ctrl.beacon_tail_buf != NULL)
			{
				kfree(pAd->cfg80211_ctrl.beacon_tail_buf);
				pAd->cfg80211_ctrl.beacon_tail_buf = NULL;
			}
			pAd->cfg80211_ctrl.beacon_tail_len = 0;
		}
			break;

                case CMD_RTPRIV_IOCTL_80211_AP_KEY_ADD:
                        CFG80211DRV_ApKeyAdd(pAd, pData);
                        break;

                case CMD_RTPRIV_IOCTL_80211_RTS_THRESHOLD_ADD:
                        CFG80211DRV_RtsThresholdAdd(pAd, Data);
                        break;

                case CMD_RTPRIV_IOCTL_80211_FRAG_THRESHOLD_ADD:
                        CFG80211DRV_FragThresholdAdd(pAd, Data);
                        break;

                case CMD_RTPRIV_IOCTL_80211_AP_KEY_DEL:
                        CFG80211DRV_ApKeyDel(pAd, pData);
                        break;

                case CMD_RTPRIV_IOCTL_80211_AP_KEY_DEFAULT_SET:
                        CFG80211_setApDefaultKey(pAd, Data);
                        break;

                case CMD_RTPRIV_IOCTL_80211_PORT_SECURED:
                        CFG80211_StaPortSecured(pAd, pData, Data);
                        break;

                case CMD_RTPRIV_IOCTL_80211_AP_STA_DEL:
                        CFG80211_ApStaDel(pAd, pData);
                        break;
#endif /* CONFIG_AP_SUPPORT */

		case CMD_RTPRIV_IOCTL_80211_CHANGE_BSS_PARM:
			CFG80211DRV_OpsChangeBssParm(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_AP_PROBE_RSP_EXTRA_IE:
			break;

		case CMD_RTPRIV_IOCTL_80211_BITRATE_SET:
			break;

        	case CMD_RTPRIV_IOCTL_80211_RESET:
            		CFG80211_reSetToDefault(pAd);
            		break;

		case CMD_RTPRIV_IOCTL_80211_VIF_ADD:
			if (CFG80211DRV_OpsVifAdd(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;

	        case CMD_RTPRIV_IOCTL_80211_VIF_DEL:
			RTMP_CFG80211_VirtualIF_Remove(pAd, pData, Data);
            		break;

#ifdef RFKILL_HW_SUPPORT
				case CMD_RTPRIV_IOCTL_80211_RFKILL:
				{
					uint32_t data = 0;
					bool active;

					/* Read GPIO pin2 as Hardware controlled radio state */
					mt76u_reg_read(pAd, GPIO_CTRL_CFG, &data);
					active = !!(data & 0x04);

					if (!active)
					{
						RTMPSetLED(pAd, LED_RADIO_OFF);
						*(UINT8 *)pData = 0;
					}
					else
						*(UINT8 *)pData = 1;
				}
					break;
#endif /* RFKILL_HW_SUPPORT */

		default:
			return NDIS_STATUS_FAILURE;
	}

	return NDIS_STATUS_SUCCESS;
}

VOID CFG80211DRV_OpsMgmtFrameProbeRegister(
        struct rtmp_adapter                             *pAd,
        VOID                                            *pData,
		bool                                          isReg)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

	/* IF Not Exist on VIF List, the device must be MAIN_DEV */
	if (isReg)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount++;
	else
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount--;

	if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount > 0)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = true;
	else
	{
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = false;
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;
	}

	DBGPRINT(RT_DEBUG_INFO, ("[%d] pAd->Cfg80211RegisterProbeReqFrame=%d[%d]\n",
		isReg, pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame,
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount));
}

VOID CFG80211DRV_OpsMgmtFrameActionRegister(
        struct rtmp_adapter                              *pAd,
        VOID                                            *pData,
		bool                                          isReg)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

	/* IF Not Exist on VIF List, the device must be MAIN_DEV */
	if (isReg)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount++;
	else
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount--;

	if (pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount > 0)
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = true;
	else
	{
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = false;
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;
	}

	DBGPRINT(RT_DEBUG_INFO, ("[%d] TYPE pAd->Cfg80211RegisterActionFrame=%d[%d]\n",
		isReg, pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame,
		pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount));
}

VOID CFG80211DRV_OpsChangeBssParm(
        struct rtmp_adapter                             *pAd,
        VOID                                            *pData)
{
	CMD_RTPRIV_IOCTL_80211_BSS_PARM *pBssInfo;
	bool TxPreamble;

	CFG80211DBG(RT_DEBUG_TRACE, ("%s\n", __FUNCTION__));

	pBssInfo = (CMD_RTPRIV_IOCTL_80211_BSS_PARM *)pData;

	/* Short Preamble */
	if (pBssInfo->use_short_preamble != -1)
	{
		CFG80211DBG(RT_DEBUG_TRACE, ("%s: ShortPreamble %d\n", __FUNCTION__, pBssInfo->use_short_preamble));
        	pAd->CommonCfg.TxPreamble = (pBssInfo->use_short_preamble == 0 ? Rt802_11PreambleLong : Rt802_11PreambleShort);
		TxPreamble = (pAd->CommonCfg.TxPreamble == Rt802_11PreambleLong ? 0 : 1);
		MlmeSetTxPreamble(pAd, (unsigned short)pAd->CommonCfg.TxPreamble);
	}

	/* CTS Protection */
	if (pBssInfo->use_cts_prot != -1)
	{
		CFG80211DBG(RT_DEBUG_TRACE, ("%s: CTS Protection %d\n", __FUNCTION__, pBssInfo->use_cts_prot));
	}

	/* Short Slot */
	if (pBssInfo->use_short_slot_time != -1)
	{
		CFG80211DBG(RT_DEBUG_TRACE, ("%s: Short Slot %d\n", __FUNCTION__, pBssInfo->use_short_slot_time));
	}
}

bool CFG80211DRV_OpsSetChannel(struct rtmp_adapter *pAd, VOID *pData)
{
	CMD_RTPRIV_IOCTL_80211_CHAN *pChan;
	UINT8 ChanId, IfType, ChannelType;
	bool FlgIsChanged;

/*
 *  enum nl80211_channel_type {
 *	NL80211_CHAN_NO_HT,
 *	NL80211_CHAN_HT20,
 *	NL80211_CHAN_HT40MINUS,
 *	NL80211_CHAN_HT40PLUS
 *  };
 */
	/* init */
	pChan = (CMD_RTPRIV_IOCTL_80211_CHAN *)pData;
	ChanId = pChan->ChanId;
	IfType = pChan->IfType;
	ChannelType = pChan->ChanType;

	if (IfType != RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* get channel BW */
		FlgIsChanged = true;

		/* set to new channel BW */
		if (ChannelType == RT_CMD_80211_CHANTYPE_HT20)
		{
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_20;
			pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_NONE;
			pAd->CommonCfg.HT_Disable = 0;
		}
		else if (ChannelType == RT_CMD_80211_CHANTYPE_HT40MINUS)
		{
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
			pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_BELOW;
			pAd->CommonCfg.HT_Disable = 0;
		}
		else if	(ChannelType == RT_CMD_80211_CHANTYPE_HT40PLUS)
		{
			/* not support NL80211_CHAN_HT40MINUS or NL80211_CHAN_HT40PLUS */
			/* i.e. primary channel = 36, secondary channel must be 40 */
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
			pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_ABOVE;
			pAd->CommonCfg.HT_Disable = 0;
		}
		else if  (ChannelType == RT_CMD_80211_CHANTYPE_NOHT)
		{
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_20;
			pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_NONE;
			pAd->CommonCfg.HT_Disable = 1;
		}

		CFG80211DBG(RT_DEBUG_TRACE, ("80211> HT Disable = %d\n", pAd->CommonCfg.HT_Disable));
	}
	else
	{
		/* for monitor mode */
		FlgIsChanged = true;
		pAd->CommonCfg.HT_Disable = 0;
		pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
	}

	if (FlgIsChanged == true)
		SetCommonHT(pAd);

	/* switch to the channel with Common Channel */
	pAd->CommonCfg.Channel = ChanId;
	pAd->MlmeAux.Channel = ChanId;

	/* CFG_TODO: for CentralChannel setting */
	//lock_channel = N_SetCenCh(pAd, pAd->CommonCfg.Channel);
	//pAd->MlmeAux.CentralChannel = lock_channel;

	//if (pAd->LatchRfRegs.Channel != pAd->CommonCfg.Channel)
	//{
	//	AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, false);
	//	AsicLockChannel(pAd, pAd->CommonCfg.Channel);
	//}

        if(pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_BELOW)
              pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 2;
        else if (pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_ABOVE)
              pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel + 2;
        else
        	pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;

	mt7612u_bbp_set_bw(pAd, pAd->CommonCfg.RegTransmitSetting.field.BW);
        AsicSwitchChannel(pAd, pAd->CommonCfg.CentralChannel,false);
        AsicLockChannel(pAd, pAd->CommonCfg.CentralChannel);

	CFG80211DBG(RT_DEBUG_TRACE, ("80211> New CH = %d, New BW = %d with Ext[%d]\n",
		pAd->CommonCfg.CentralChannel, pAd->CommonCfg.RegTransmitSetting.field.BW,
		pAd->CommonCfg.RegTransmitSetting.field.EXTCHA));

	if(IfType == RT_CMD_80211_IFTYPE_AP)
	{
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> Set the channel in AP Mode\n"));
		return true;
	}
#ifdef CONFIG_STA_SUPPORT
	if ((IfType == RT_CMD_80211_IFTYPE_STATION) && (FlgIsChanged == true))
	{
		/*
			1. Station mode;
			2. New BW settings is 20MHz but current BW is not 20MHz;
			3. New BW settings is 40MHz but current BW is 20MHz;

			Re-connect to the AP due to BW 20/40 or HT/non-HT change.
		*/
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> Set the channel in STA Mode\n"));
	}

	if (IfType == RT_CMD_80211_IFTYPE_ADHOC)
	{
		/* update IBSS beacon */
		MlmeUpdateTxRates(pAd, false, 0);
		MakeIbssBeacon(pAd);
		AsicEnableIbssSync(pAd);

		Set_SSID_Proc(pAd, (char *)pAd->CommonCfg.Ssid);
	}

	if (IfType == RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* reset monitor mode in the new channel */
		Set_NetworkType_Proc(pAd, "Monitor");
		mt76u_reg_write(pAd, RX_FILTR_CFG, pChan->MonFilterFlag);
	}
#endif /*CONFIG_STA_SUPPORT*/
	return true;
}

bool CFG80211DRV_OpsJoinIbss(
	struct rtmp_adapter				*pAd,
	VOID						*pData)
{
#ifdef CONFIG_STA_SUPPORT
	CMD_RTPRIV_IOCTL_80211_IBSS *pIbssInfo;


	pIbssInfo = (CMD_RTPRIV_IOCTL_80211_IBSS *)pData;
	pAd->StaCfg.bAutoReconnect = true;

	pAd->CommonCfg.BeaconPeriod = pIbssInfo->BeaconInterval;
	Set_SSID_Proc(pAd, (char *)pIbssInfo->pSsid);
#endif /* CONFIG_STA_SUPPORT */
	return true;
}

bool CFG80211DRV_OpsLeave(
	struct rtmp_adapter				*pAd,
	UINT8						IfType)
{
#ifdef CONFIG_STA_SUPPORT
	MLME_DEAUTH_REQ_STRUCT   DeAuthReq;
	MLME_QUEUE_ELEM *pMsgElem = NULL;

	pAd->StaCfg.bAutoReconnect = false;
	pAd->cfg80211_ctrl.FlgCfg80211Connecting = false;

	pAd->MlmeAux.AutoReconnectSsidLen= 32;
	memset(pAd->MlmeAux.AutoReconnectSsid, 0, pAd->MlmeAux.AutoReconnectSsidLen);

	pMsgElem = kmalloc(sizeof(MLME_QUEUE_ELEM), GFP_ATOMIC);

	COPY_MAC_ADDR(DeAuthReq.Addr, pAd->CommonCfg.Bssid);

	DeAuthReq.Reason = REASON_DEAUTH_STA_LEAVING;
	pMsgElem->MsgLen = sizeof(MLME_DEAUTH_REQ_STRUCT);
	memmove(pMsgElem->Msg, &DeAuthReq, sizeof(MLME_DEAUTH_REQ_STRUCT));
	MlmeDeauthReqAction(pAd, pMsgElem);
	kfree(pMsgElem);
	pMsgElem = NULL;

	LinkDown(pAd, false);

#endif /* CONFIG_STA_SUPPORT */
	return true;
}


bool CFG80211DRV_StaGet(
	struct rtmp_adapter				*pAd,
	VOID						*pData)
{
	CMD_RTPRIV_IOCTL_80211_STA *pIbssInfo;

	pIbssInfo = (CMD_RTPRIV_IOCTL_80211_STA *)pData;

#ifdef CONFIG_AP_SUPPORT
{
	MAC_TABLE_ENTRY *pEntry;
	uint32_t DataRate = 0;
	uint32_t RSSI;


	pEntry = MacTableLookup(pAd, pIbssInfo->MAC);
	if (pEntry == NULL)
		return false;

	/* fill tx rate */
	//getRate(pEntry->HTPhyMode, &DataRate);
	RtmpDrvRateGet(pAd, pEntry->HTPhyMode.field.MODE, pEntry->HTPhyMode.field.ShortGI,
				 pEntry->HTPhyMode.field.BW,pEntry->HTPhyMode.field.MCS,
				 newRateGetAntenna(pEntry->MaxHTPhyMode.field.MCS),&DataRate);
	DataRate /= 500000;
	DataRate /= 2;

	if ((pEntry->HTPhyMode.field.MODE == MODE_HTMIX) ||
		(pEntry->HTPhyMode.field.MODE == MODE_HTGREENFIELD))
	{
		if (pEntry->HTPhyMode.field.BW)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_BW_40;

		if (pEntry->HTPhyMode.field.ShortGI)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_SHORT_GI;

		pIbssInfo->TxRateMCS = pEntry->HTPhyMode.field.MCS;
	}
	else
	{
		pIbssInfo->TxRateFlags = RT_CMD_80211_TXRATE_LEGACY;
		pIbssInfo->TxRateMCS = DataRate*10; /* unit: 100kbps */
	}

	/* fill signal */
	RSSI = RTMPAvgRssi(pAd, &pEntry->RssiSample);
	pIbssInfo->Signal = RSSI;

	/* fill tx count */
	pIbssInfo->TxPacketCnt = pEntry->OneSecTxNoRetryOkCount +
						pEntry->OneSecTxRetryOkCount +
						pEntry->OneSecTxFailCount;

	/* fill inactive time */
	pIbssInfo->InactiveTime = pEntry->NoDataIdleCount * 1000; /* unit: ms */
	pIbssInfo->InactiveTime *= MLME_TASK_EXEC_MULTIPLE;
	pIbssInfo->InactiveTime /= 20;
}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
{
	HTTRANSMIT_SETTING PhyInfo;
	uint32_t DataRate = 0;
	uint32_t RSSI;


	/* fill tx rate */
    if ((!WMODE_CAP_N(pAd->CommonCfg.PhyMode)) ||
	 (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
	{
		PhyInfo.word = pAd->StaCfg.wdev.HTPhyMode.word;
	}
    else
		PhyInfo.word = pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word;

	//getRate(PhyInfo, &DataRate);
	RtmpDrvRateGet(pAd, PhyInfo.field.MODE, PhyInfo.field.ShortGI,
				 PhyInfo.field.BW,PhyInfo.field.MCS,
				 newRateGetAntenna(PhyInfo.field.MCS),&DataRate);
	DataRate /= 500000;
	DataRate /= 2;

	if ((PhyInfo.field.MODE == MODE_HTMIX) ||
		(PhyInfo.field.MODE == MODE_HTGREENFIELD))
	{
		if (PhyInfo.field.BW)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_BW_40;

		if (PhyInfo.field.ShortGI)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_SHORT_GI;

		pIbssInfo->TxRateMCS = PhyInfo.field.MCS;
	}
	else
	{
		pIbssInfo->TxRateFlags = RT_CMD_80211_TXRATE_LEGACY;
		pIbssInfo->TxRateMCS = DataRate*10; /* unit: 100kbps */
	}

	/* fill signal */
	RSSI = RTMPAvgRssi(pAd, &pAd->StaCfg.RssiSample);
	pIbssInfo->Signal = RSSI;
}
#endif /* CONFIG_STA_SUPPORT */

	return true;
}

bool CFG80211DRV_StaKeyAdd(
	struct rtmp_adapter				*pAd,
	VOID						*pData)
{
#ifdef CONFIG_STA_SUPPORT
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;


	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;
	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40 || pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("RT_CMD_80211_KEY_WEP\n"));
	}
	else
	{
		RT_CMD_STA_IOCTL_SECURITY IoctlSec;

		DBGPRINT(RT_DEBUG_TRACE, ("Set_WPAPSK_Proc ==> %d, %d, %d...\n", pKeyInfo->KeyId, pKeyInfo->KeyType, (int) strlen(pKeyInfo->KeyBuf)));

		IoctlSec.KeyIdx = pKeyInfo->KeyId;
		IoctlSec.pData = pKeyInfo->KeyBuf;
		IoctlSec.length = pKeyInfo->KeyLen;

		/* YF@20120327: Due to WepStatus will be set in the cfg connect function.*/
		if (pAd->StaCfg.wdev.WepStatus == Ndis802_11Encryption2Enabled)
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
		else if (pAd->StaCfg.wdev.WepStatus == Ndis802_11Encryption3Enabled)
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;
		IoctlSec.flags = RT_CMD_STA_IOCTL_SECURITY_ENABLED;
		if (pKeyInfo->bPairwise == false )
		{
			if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			DBGPRINT(RT_DEBUG_TRACE, ("Install GTK: %d\n", IoctlSec.Alg));
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_GROUP_KEY;
		}
		else
		{
			if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			DBGPRINT(RT_DEBUG_TRACE, ("Install PTK: %d\n", IoctlSec.Alg));
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_SET_TX_KEY;
		}

		/*Set_GroupKey_Proc(pAd, &IoctlSec) */
		RTMP_STA_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_STA_SIOCSIWENCODEEXT, 0,
							  &IoctlSec, 0, INT_MAIN);
	}
#endif /* CONFIG_STA_SUPPORT */

	return true;
}

bool CFG80211DRV_Connect(
	struct rtmp_adapter				*pAd,
	VOID						*pData)
{
#ifdef CONFIG_STA_SUPPORT
	CMD_RTPRIV_IOCTL_80211_CONNECT *pConnInfo;
	u8 SSID[NDIS_802_11_LENGTH_SSID + 1]; /* Add One for SSID_Len == 32 */
	uint32_t SSIDLen;
	RT_CMD_STA_IOCTL_SECURITY_ADV IoctlWpa;

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_INFRA_ON) &&
            OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211: Connected, disconnect first !\n"));
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211: No Connection\n"));
	}

	pConnInfo = (CMD_RTPRIV_IOCTL_80211_CONNECT *)pData;

	/* change to infrastructure mode if we are in ADHOC mode */
	Set_NetworkType_Proc(pAd, "Infra");

	SSIDLen = pConnInfo->SsidLen;
	if (SSIDLen > NDIS_802_11_LENGTH_SSID)
	{
		SSIDLen = NDIS_802_11_LENGTH_SSID;
	}

	memset(&SSID, 0, sizeof(SSID));
	memcpy(SSID, pConnInfo->pSsid, SSIDLen);

	if (pConnInfo->bWpsConnection)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WPS Connection onGoing.....\n"));
		/* YF@20120327: Trigger Driver to Enable WPS function. */
		pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP |= WPA_SUPPLICANT_ENABLE_WPS;  /* Set_Wpa_Support(pAd, "3") */
		Set_AuthMode_Proc(pAd, "OPEN");
		Set_EncrypType_Proc(pAd, "NONE");
		Set_SSID_Proc(pAd, (char *)SSID);

		return true;
	}
	else
	{
		pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE; /* Set_Wpa_Support(pAd, "1")*/
	}

	/* set authentication mode */
	if (pConnInfo->WpaVer == 2)
	{
		if (pConnInfo->FlgIs8021x == true) {
			DBGPRINT(RT_DEBUG_TRACE, ("WPA2\n"));
			Set_AuthMode_Proc(pAd, "WPA2");
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("WPA2PSK\n"));
			Set_AuthMode_Proc(pAd, "WPA2PSK");
		}
	}
	else if (pConnInfo->WpaVer == 1)
	{
		if (pConnInfo->FlgIs8021x == true) {
			DBGPRINT(RT_DEBUG_TRACE, ("WPA\n"));
			Set_AuthMode_Proc(pAd, "WPA");
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("WPAPSK\n"));
			Set_AuthMode_Proc(pAd, "WPAPSK");
		}
	}
	else if (pConnInfo->AuthType == Ndis802_11AuthModeAutoSwitch)
		Set_AuthMode_Proc(pAd, "WEPAUTO");
    else if (pConnInfo->AuthType == Ndis802_11AuthModeShared)
		Set_AuthMode_Proc(pAd, "SHARED");
	else
		Set_AuthMode_Proc(pAd, "OPEN");

	CFG80211DBG(RT_DEBUG_TRACE,
				("80211> AuthMode = %d\n", pAd->StaCfg.wdev.AuthMode));

	/* set encryption mode */
	if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_CCMP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("AES\n"));
		Set_EncrypType_Proc(pAd, "AES");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_TKIP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("TKIP\n"));
		Set_EncrypType_Proc(pAd, "TKIP");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_WEP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WEP\n"));
		Set_EncrypType_Proc(pAd, "WEP");
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("NONE\n"));
		Set_EncrypType_Proc(pAd, "NONE");
	}

	/* Groupwise Key Information Setting */
	IoctlWpa.flags = RT_CMD_STA_IOCTL_WPA_GROUP;
	if (pConnInfo->GroupwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_CCMP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("GTK AES\n"));
		IoctlWpa.value = RT_CMD_STA_IOCTL_WPA_GROUP_CCMP;
		RtmpIoctl_rt_ioctl_siwauth(pAd, &IoctlWpa, 0);
	}
	else if (pConnInfo->GroupwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_TKIP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("GTK TKIP\n"));
		IoctlWpa.value = RT_CMD_STA_IOCTL_WPA_GROUP_TKIP;
		RtmpIoctl_rt_ioctl_siwauth(pAd, &IoctlWpa, 0);
	}

	CFG80211DBG(RT_DEBUG_TRACE,
				("80211> EncrypType = %d\n", pAd->StaCfg.wdev.WepStatus));

	CFG80211DBG(RT_DEBUG_TRACE, ("80211> Key = %s\n", pConnInfo->pKey));

	/* set channel: STATION will auto-scan */

	/* set WEP key */
	if (pConnInfo->pKey &&
		((pConnInfo->GroupwiseEncrypType | pConnInfo->PairwiseEncrypType) &
												RT_CMD_80211_CONN_ENCRYPT_WEP))
	{
		u8 KeyBuf[50];

		/* reset AuthMode and EncrypType */
		Set_EncrypType_Proc(pAd, "WEP");

		/* reset key */
#ifdef RT_CFG80211_DEBUG
		hex_dump("KeyBuf=", (UINT8 *)pConnInfo->pKey, pConnInfo->KeyLen);
#endif /* RT_CFG80211_DEBUG */

		pAd->StaCfg.wdev.DefaultKeyId = pConnInfo->KeyIdx; /* base 0 */
		if (pConnInfo->KeyLen >= sizeof(KeyBuf))
			return false;

		memcpy(KeyBuf, pConnInfo->pKey, pConnInfo->KeyLen);
		KeyBuf[pConnInfo->KeyLen] = 0x00;

		CFG80211DBG(RT_DEBUG_ERROR,
					("80211> pAd->StaCfg.DefaultKeyId = %d\n",
					pAd->StaCfg.wdev.DefaultKeyId));

		Set_Wep_Key_Proc(pAd, (char *)KeyBuf, (INT)pConnInfo->KeyLen, (INT)pConnInfo->KeyIdx);

	} /* End of if */

	/* TODO: We need to provide a command to set BSSID to associate a AP */
	pAd->cfg80211_ctrl.FlgCfg80211Connecting = true;


	Set_SSID_Proc(pAd, (char *)SSID);
	CFG80211DBG(RT_DEBUG_TRACE, ("80211> Connecting SSID = %s\n", SSID));
#endif /* CONFIG_STA_SUPPORT */

	return true;
}

VOID CFG80211DRV_SurveyGet(
	struct rtmp_adapter				*pAd,
	VOID						*pData)
{
	CMD_RTPRIV_IOCTL_80211_SURVEY *pSurveyInfo;


	pSurveyInfo = (CMD_RTPRIV_IOCTL_80211_SURVEY *)pData;

	pSurveyInfo->pCfg80211 = pAd->pCfg80211_CB;

#ifdef AP_QLOAD_SUPPORT
	pSurveyInfo->ChannelTimeBusy = pAd->phy_ctrl.QloadLatestChannelBusyTimePri;
	pSurveyInfo->ChannelTimeExtBusy = pAd->phy_ctrl.QloadLatestChannelBusyTimeSec;
#endif /* AP_QLOAD_SUPPORT */
}


VOID CFG80211_UnRegister(
	IN struct rtmp_adapter				*pAd,
	IN VOID						*pNetDev)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;

	/* sanity check */
	if (pAd->pCfg80211_CB == NULL)
		return;


	CFG80211OS_UnRegister(pAd->pCfg80211_CB, pNetDev);
	RTMP_DRIVER_80211_SCAN_STATUS_LOCK_INIT(pAd, false);

	/* Reset CFG80211 Global Setting Here */
	DBGPRINT(RT_DEBUG_TRACE, ("==========> TYPE Reset CFG80211 Global Setting Here <==========\n"));
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = false,
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;

	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = false;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;

	pAd->pCfg80211_CB = NULL;
	pAd->CommonCfg.HT_Disable = 0;

    	/* It should be free when ScanEnd,
      	   But Hit close the device in Scanning */
     	if (pCfg80211_ctrl->pCfg80211ChanList != NULL)
     	{
     		kfree(pCfg80211_ctrl->pCfg80211ChanList);
        	pCfg80211_ctrl->pCfg80211ChanList = NULL;
     	}

  	pCfg80211_ctrl->Cfg80211ChanListLen = 0;
	pCfg80211_ctrl->Cfg80211CurChanIndex = 0;

	if(pCfg80211_ctrl->pExtraIe)
	{
		kfree(pCfg80211_ctrl->pExtraIe);
		pCfg80211_ctrl->pExtraIe = NULL;
	}
	pCfg80211_ctrl->ExtraIeLen = 0;

/*
CFG_TODO
     if (pAd->pTxStatusBuf != NULL)
     {
         kfree(pAd->pTxStatusBuf);
         pAd->pTxStatusBuf = NULL;
     }
	 pAd->TxStatusBufLen = 0;
*/
#ifdef CONFIG_AP_SUPPORT
    if (pAd->cfg80211_ctrl.beacon_tail_buf != NULL)
    {
        kfree(pAd->cfg80211_ctrl.beacon_tail_buf);
        pAd->cfg80211_ctrl.beacon_tail_buf = NULL;
    }
	pAd->cfg80211_ctrl.beacon_tail_len = 0;

#endif /* CONFIG_AP_SUPPORT */

}


/*
========================================================================
Routine Description:
	Parse and handle country region in beacon from associated AP.

Arguments:
	pAdCB			- WLAN control block pointer
	pVIE			- Beacon elements
	LenVIE			- Total length of Beacon elements

Return Value:
	NONE

Note:
========================================================================
*/
VOID CFG80211_BeaconCountryRegionParse(
	IN struct rtmp_adapter		*pAd,
	IN NDIS_802_11_VARIABLE_IEs	*pVIE,
	IN uint16_t 				LenVIE)
{
	u8 *pElement = (u8 *)pVIE;
	uint32_t LenEmt;


	while(LenVIE > 0)
	{
		pVIE = (NDIS_802_11_VARIABLE_IEs *)pElement;

		if (pVIE->ElementID == IE_COUNTRY)
		{
			/* send command to do regulation hint only when associated */
			 RT_CFG80211_CRDA_REG_HINT11D(pAd, pVIE->data, pVIE->Length);
			//RTEnqueueInternalCmd(pAd, CMDTHREAD_REG_HINT_11D,
			//					pVIE->data, pVIE->Length);
			break;
		}

		LenEmt = pVIE->Length + 2;

		if (LenVIE <= LenEmt)
			break; /* length is not enough */

		pElement += LenEmt;
		LenVIE -= LenEmt;
	}
} /* End of CFG80211_BeaconCountryRegionParse */

/*
========================================================================
Routine Description:
	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.

Arguments:
	pAdCB			- WLAN control block pointer

Return Value:
	NONE

Note:
	CFG80211_SupBandInit() is called in xx_probe().
========================================================================
*/

#ifdef CONFIG_STA_SUPPORT
VOID CFG80211_LostApInform(
    IN struct rtmp_adapter		*pAd)
{

	struct mt7612u_cfg80211_cb *p80211CB = pAd->pCfg80211_CB;

	DBGPRINT(RT_DEBUG_TRACE, ("80211> CFG80211_LostApInform ==> %d\n",
					p80211CB->sme_state));
	pAd->StaCfg.bAutoReconnect = false;

	if (p80211CB->sme_state == SME_CONNECTING) {
		   cfg80211_connect_result(pAd->net_dev, NULL, NULL, 0, NULL, 0,
								   WLAN_STATUS_UNSPECIFIED_FAILURE, GFP_KERNEL);
	} else if (p80211CB->sme_state == SME_CONNECTED) {
		   cfg80211_disconnected(pAd->net_dev, 0, NULL, 0, true, GFP_KERNEL);
	}
}
#endif /*CONFIG_STA_SUPPORT*/


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from driver.

Arguments:
	pAd				- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
VOID CFG80211_RegHint(
	IN struct rtmp_adapter				*pAd,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen)
{
	CFG80211OS_RegHint(CFG80211CB, pCountryIe, CountryIeLen);
}


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from country element.

Arguments:
	pAdCB			- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
VOID CFG80211_RegHint11D(
	IN struct rtmp_adapter				*pAd,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */

	CFG80211OS_RegHint11D(CFG80211CB, pCountryIe, CountryIeLen);
}


/*
========================================================================
Routine Description:
	Apply new regulatory rule.

Arguments:
	pAdCB			- WLAN control block pointer
	pWiphy			- Wireless hardware description
	pAlpha2			- Regulation domain (2B)

Return Value:
	NONE

Note:
	Can only be called when interface is up.

	For general mac80211 device, it will be set to new power by Ops->config()
	In rt2x00/, the settings is done in rt2x00lib_config().
========================================================================
*/
VOID CFG80211_RegRuleApply(
	IN struct rtmp_adapter				*pAd,
	IN VOID						*pWiphy,
	IN u8 				*pAlpha2)
{
	VOID *pBand24G, *pBand5G;
	uint32_t IdBand, IdChan, IdPwr;
	uint32_t ChanNum, ChanId, Power, RecId, DfsType;
	bool FlgIsRadar;


	CFG80211DBG(RT_DEBUG_TRACE, ("crda> CFG80211_RegRuleApply ==>\n"));

	/* init */
	pBand24G = NULL;
	pBand5G = NULL;

	if (pAd == NULL)
		return;

	spin_lock_bh(&pAd->irq_lock);

	/* zero first */
	memset(pAd->ChannelList, 0,
					MAX_NUM_OF_CHANNELS * sizeof(struct CHANNEL_TX_POWER));

	/* 2.4GHZ & 5GHz */
	RecId = 0;

	/* find the DfsType */
	DfsType = CE;

	pBand24G = NULL;
	pBand5G = NULL;

	if (CFG80211OS_BandInfoGet(CFG80211CB, pWiphy, &pBand24G, &pBand5G) == false)
		return;


	for(IdBand=0; IdBand<2; IdBand++)
	{
		if (((IdBand == 0) && (pBand24G == NULL)) ||
			((IdBand == 1) && (pBand5G == NULL)))
		{
			continue;
		}

		if (IdBand == 0)
		{
			CFG80211DBG(RT_DEBUG_TRACE, ("crda> reset chan/power for 2.4GHz\n"));
		}
		else
		{
			CFG80211DBG(RT_DEBUG_TRACE, ("crda> reset chan/power for 5GHz\n"));
		}

		ChanNum = CFG80211OS_ChanNumGet(CFG80211CB, pWiphy, IdBand);

		for(IdChan=0; IdChan<ChanNum; IdChan++)
		{
			if (CFG80211OS_ChanInfoGet(CFG80211CB, pWiphy, IdBand, IdChan,
									&ChanId, &Power, &FlgIsRadar) == false)
			{
				/* the channel is not allowed in the regulatory domain */
				/* get next channel information */
				continue;
			}

			if (!WMODE_CAP_2G(pAd->CommonCfg.PhyMode))
			{
				/* 5G-only mode */
				if (ChanId <= CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}

			if (!WMODE_CAP_5G(pAd->CommonCfg.PhyMode))
			{
				/* 2.4G-only mode */
				if (ChanId > CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}

			for(IdPwr=0; IdPwr<MAX_NUM_OF_CHANNELS; IdPwr++)
			{
				if (ChanId == pAd->TxPower[IdPwr].Channel)
				{
					/* init the channel info. */
					memmove(&pAd->ChannelList[RecId],
									&pAd->TxPower[IdPwr],
									sizeof(struct CHANNEL_TX_POWER));

					/* keep channel number */
					pAd->ChannelList[RecId].Channel = ChanId;

					/* keep maximum tranmission power */
					pAd->ChannelList[RecId].MaxTxPwr = Power;

					/* keep DFS flag */
					if (FlgIsRadar == true)
						pAd->ChannelList[RecId].DfsReq = true;
					else
						pAd->ChannelList[RecId].DfsReq = false;

					/* keep DFS type */
					pAd->ChannelList[RecId].RegulatoryDomain = DfsType;

					/* re-set DFS info. */
					pAd->CommonCfg.RDDurRegion = DfsType;

					CFG80211DBG(RT_DEBUG_TRACE,
								("Chan %03d:\tpower %d dBm, "
								"DFS %d, DFS Type %d\n",
								ChanId, Power,
								((FlgIsRadar == true)?1:0),
								DfsType));

					/* change to record next channel info. */
					RecId ++;
					break;
				}
			}
		}
	}

	pAd->ChannelListNum = RecId;
	spin_unlock_bh(&pAd->irq_lock);

	CFG80211DBG(RT_DEBUG_TRACE, ("crda> Number of channels = %d\n", RecId));
} /* End of CFG80211_RegRuleApply */

/*
========================================================================
Routine Description:
	Inform CFG80211 about association status.

Arguments:
	pAdCB			- WLAN control block pointer
	pBSSID			- the BSSID of the AP
	pReqIe			- the element list in the association request frame
	ReqIeLen		- the request element length
	pRspIe			- the element list in the association response frame
	RspIeLen		- the response element length
	FlgIsSuccess	- 1: success; otherwise: fail

Return Value:
	None

Note:
========================================================================
*/
VOID CFG80211_ConnectResultInform(
	IN struct rtmp_adapter				*pAd,
	IN u8 				*pBSSID,
	IN u8 				*pReqIe,
	IN uint32_t 				ReqIeLen,
	IN u8 				*pRspIe,
	IN uint32_t 				RspIeLen,
	IN u8 				FlgIsSuccess)
{
	CFG80211DBG(RT_DEBUG_TRACE, ("80211> CFG80211_ConnectResultInform ==>\n"));

	CFG80211OS_ConnectResultInform(CFG80211CB,
								pBSSID,
								pReqIe,
								ReqIeLen,
								pRspIe,
								RspIeLen,
								FlgIsSuccess);

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = false;
} /* End of CFG80211_ConnectResultInform */




/*
========================================================================
Routine Description:
	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.

Arguments:
	pAdCB			- WLAN control block pointer

Return Value:
	true			- re-init successfully
	false			- re-init fail

Note:
	CFG80211_SupBandInit() is called in xx_probe().
	But we do not have complete chip information in xx_probe() so we
	need to re-init bands in xx_open().
========================================================================
*/
bool CFG80211_SupBandReInit(
	IN struct rtmp_adapter				*pAd)
{
	CFG80211_BAND BandInfo;


	CFG80211DBG(RT_DEBUG_TRACE, ("80211> re-init bands...\n"));

	/* re-init bands */
	memset(&BandInfo, 0, sizeof(BandInfo));
	CFG80211_BANDINFO_FILL(pAd, &BandInfo);

	return CFG80211OS_SupBandReInit(CFG80211CB, &BandInfo);
} /* End of CFG80211_SupBandReInit */

#ifdef CONFIG_STA_SUPPORT
//CMD_RTPRIV_IOCTL_80211_KEY_DEFAULT_SET:
INT CFG80211_setStaDefaultKey(
	IN struct rtmp_adapter                   *pAd,
	IN UINT 				Data
)
{

	DBGPRINT(RT_DEBUG_TRACE, ("Set Sta Default Key: %d\n", Data));
    pAd->StaCfg.wdev.DefaultKeyId = Data; /* base 0 */
	return 0;
}


#endif /*CONFIG_STA_SUPPORT*/
INT CFG80211_reSetToDefault(
	IN struct rtmp_adapter                       *pAd)
{
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	DBGPRINT(RT_DEBUG_TRACE, (" %s\n", __FUNCTION__));
#ifdef CONFIG_STA_SUPPORT
	/* Driver Internal Parm */
	pAd->StaCfg.bAutoConnectByBssid = false;
#endif /*CONFIG_STA_SUPPORT*/
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterProbeReqFrame = false;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211RegisterActionFrame = false;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ProbeReqCount = 0;
	pCfg80211_ctrl->cfg80211MainDev.Cfg80211ActionCount = 0;

	pCfg80211_ctrl->Cfg80211RocTimerInit = false;
	pCfg80211_ctrl->Cfg80211RocTimerRunning = false;
	pCfg80211_ctrl->FlgCfg80211Scanning = false;

	return true;
}

//CFG_TODO
u8 CFG80211_getCenCh(struct rtmp_adapter *pAd, u8 prim_ch)
{
	u8 ret_channel;

	if (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40)
	{
		if (pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_ABOVE)
			ret_channel = prim_ch + 2;
		else
		{
			if (prim_ch == 14)
				ret_channel = prim_ch - 1;
			else
				ret_channel = prim_ch - 2;
		}
	}
	else
		ret_channel = prim_ch;

	return ret_channel;
}

#endif /* RT_CFG80211_SUPPORT */

